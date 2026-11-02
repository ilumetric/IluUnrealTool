#include "IluAssetEditorToolset.h"
#include "IluUnrealToolCore.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "GraphEditor.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace IluAssetEditor
{
	// Редактор ищем через UAssetEditorSubsystem, виджеты — обходом дерева Slate его главной вкладки: так не нужны модули
	// самих редакторов (PCGEditor, ProceduralVegetationEditor и т. п.), а графы и вьюпорты у всех штатные.
	static IAssetEditorInstance* FindEditor(UObject* Asset, FString& OutError)
	{
		if (!Asset)
		{
			OutError = TEXT("Error: Asset is null.");
			return nullptr;
		}
		UAssetEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		IAssetEditorInstance* Editor = Subsystem ? Subsystem->FindEditorForAsset(Asset, /*bFocusIfOpen*/ true) : nullptr;
		if (!Editor)
		{
			OutError = FString::Printf(TEXT("Error: no open editor for %s — open it first (EditorAppToolset.OpenEditorForAsset)."),
				*Asset->GetPathName());
		}
		return Editor;
	}

	static TSharedPtr<SWidget> GetEditorRoot(IAssetEditorInstance* Editor, FString& OutError)
	{
		const TSharedPtr<FTabManager> TabManager = Editor->GetAssociatedTabManager();
		const TSharedPtr<SDockTab> OwnerTab = TabManager.IsValid() ? TabManager->GetOwnerTab() : nullptr;
		if (!OwnerTab.IsValid())
		{
			OutError = TEXT("Error: the editor has no tab with widgets (is it fully open?).");
			return nullptr;
		}
		return OwnerTab->GetContent();
	}

	static void CollectWidgets(const TSharedRef<SWidget>& Widget, const FName Type, TArray<TSharedRef<SWidget>>& OutWidgets)
	{
		if (Widget->GetType() == Type)
		{
			OutWidgets.Add(Widget);
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			CollectWidgets(Children->GetChildAt(Index), Type, OutWidgets);
		}
	}

	// Сравнение без учёта регистра и '_' / ' ': PCG показывает имя меша Export как «SK Vegetation Pine 02 A».
	static FString Normalize(const FString& Text)
	{
		return Text.Replace(TEXT("_"), TEXT(" ")).TrimStartAndEnd().ToLower();
	}

	static FString GetTitle(const UEdGraphNode* Node)
	{
		return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	}

	// Имя ноды редактора, её заголовок или имя объекта модели, на который она ссылается (UPCGEditorGraphNodeBase::PCGNode,
	// UMaterialGraphNode::MaterialExpression, …) — тот же Export_0, что в PCGToolset.GetGraphStructure.
	static bool Matches(const UEdGraphNode* Node, const FString& Query)
	{
		const FString Wanted = Normalize(Query);
		if (Normalize(Node->GetName()) == Wanted || Normalize(GetTitle(Node)) == Wanted)
		{
			return true;
		}
		for (TFieldIterator<FObjectPropertyBase> It(Node->GetClass()); It; ++It)
		{
			const UObject* Value = It->GetObjectPropertyValue_InContainer(Node);
			if (Value && Normalize(Value->GetName()) == Wanted)
			{
				return true;
			}
		}
		return false;
	}

	static FString DescribeNode(const UEdGraphNode* Node)
	{
		for (TFieldIterator<FObjectPropertyBase> It(Node->GetClass()); It; ++It)
		{
			const UObject* Value = It->GetObjectPropertyValue_InContainer(Node);
			if (Value && Value->GetOuter() == Node->GetGraph()->GetOuter())
			{
				return FString::Printf(TEXT("%s '%s'"), *Value->GetName(), *GetTitle(Node));
			}
		}
		return FString::Printf(TEXT("%s '%s'"), *Node->GetName(), *GetTitle(Node));
	}

	// Текст из SRichTextBlock приходит с разметкой стилей: <TextBlock.ShadowedText>Points: 10</>.
	static FString StripMarkup(const FString& Text)
	{
		FString Result;
		bool bInTag = false;
		for (const TCHAR Char : Text)
		{
			if (Char == TEXT('<'))
			{
				bInTag = true;
			}
			else if (Char == TEXT('>') && bInTag)
			{
				bInTag = false;
			}
			else if (!bInTag)
			{
				Result.AppendChar(Char);
			}
		}
		return Result;
	}

	static void CollectTexts(const TSharedRef<SWidget>& Root, TArray<FString>& OutLines)
	{
		TArray<TSharedRef<SWidget>> Blocks;
		CollectWidgets(Root, TEXT("STextBlock"), Blocks);
		TArray<TSharedRef<SWidget>> RichBlocks;
		CollectWidgets(Root, TEXT("SRichTextBlock"), RichBlocks);
		TArray<FString> Texts;
		for (const TSharedRef<SWidget>& Block : Blocks)
		{
			Texts.Add(StaticCastSharedRef<STextBlock>(Block)->GetText().ToString());
		}
		for (const TSharedRef<SWidget>& Block : RichBlocks)
		{
			Texts.Add(StripMarkup(StaticCastSharedRef<SRichTextBlock>(Block)->GetText().ToString()));
		}
		for (const FString& Text : Texts)
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					OutLines.AddUnique(Trimmed);
				}
			}
		}
	}

	// Вьюпорт превью — тот, что показывает не мир уровня. Все SViewport редакторов ассетов — FSceneViewport с клиентом
	// FEditorViewportClient (у PCG / PVE — его наследник).
	static FSceneViewport* FindPreviewViewport(const TSharedRef<SWidget>& Root, TSharedPtr<SWidget>& OutViewportWidget)
	{
		TArray<TSharedRef<SWidget>> Viewports;
		CollectWidgets(Root, TEXT("SViewport"), Viewports);
		const UWorld* LevelWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		for (const TSharedRef<SWidget>& Widget : Viewports)
		{
			const TSharedPtr<ISlateViewport> SlateViewport = StaticCastSharedRef<SViewport>(Widget)->GetViewportInterface().Pin();
			if (!SlateViewport.IsValid())
			{
				continue;
			}
			FSceneViewport* SceneViewport = static_cast<FSceneViewport*>(SlateViewport.Get());
			const FViewportClient* Client = SceneViewport->GetClient();
			const UWorld* World = Client ? Client->GetWorld() : nullptr;
			if (World && World != LevelWorld && SceneViewport->GetSizeXY().GetMin() > 0)
			{
				OutViewportWidget = Widget;
				return SceneViewport;
			}
		}
		return nullptr;
	}

	// Видимое содержимое превью: без пола (bSelectable = false у FAdvancedPreviewScene), неба и прочих мешей
	// /Engine/EditorMeshes и без «бесконечных» компонентов (линии отладки и т. п.).
	static FBox GetContentBounds(const UWorld* World)
	{
		FBox Box(ForceInit);
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			const UPrimitiveComponent* Component = *It;
			if (Component->GetWorld() != World || !Component->IsRegistered() || !Component->IsVisible() || !Component->bSelectable)
			{
				continue;
			}
			if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
				if (Mesh && Mesh->GetPathName().StartsWith(TEXT("/Engine/EditorMeshes/")))
				{
					continue;
				}
			}
			const FBoxSphereBounds& Bounds = Component->Bounds;
			if (Bounds.SphereRadius <= KINDA_SMALL_NUMBER || Bounds.SphereRadius > 100000.0f)
			{
				continue;
			}
			Box += Bounds.GetBox();
		}
		return Box;
	}

	// Расстояние, при котором все 8 углов коробки попадают в кадр (а не описанная сфера — так кадр плотнее).
	static double FitDistance(const FBox& Box, const FRotator& Rotation, double HalfFovX, double HalfFovY)
	{
		const FRotationMatrix Axes(Rotation);
		const FVector Forward = Axes.GetUnitAxis(EAxis::X);
		const FVector Right = Axes.GetUnitAxis(EAxis::Y);
		const FVector Up = Axes.GetUnitAxis(EAxis::Z);
		const FVector Center = Box.GetCenter();
		FVector Corners[8];
		Box.GetVertices(Corners);
		double Distance = 0.0;
		for (const FVector& Corner : Corners)
		{
			const FVector Local = Corner - Center;
			const double Depth = Local | Forward;
			Distance = FMath::Max(Distance, FMath::Abs(Local | Right) / FMath::Tan(HalfFovX) - Depth);
			Distance = FMath::Max(Distance, FMath::Abs(Local | Up) / FMath::Tan(HalfFovY) - Depth);
		}
		return Distance * 1.05 + 10.0;
	}
}

FString UIluAssetEditorToolset::SelectGraphNodes(UObject* Asset, const TArray<FString>& Nodes, bool bFrame)
{
	FString Error;
	IAssetEditorInstance* Editor = IluAssetEditor::FindEditor(Asset, Error);
	const TSharedPtr<SWidget> Root = Editor ? IluAssetEditor::GetEditorRoot(Editor, Error) : nullptr;
	if (!Root.IsValid())
	{
		return Error;
	}
	if (Nodes.IsEmpty())
	{
		return TEXT("Error: Nodes is empty.");
	}

	TArray<TSharedRef<SWidget>> GraphEditors;
	IluAssetEditor::CollectWidgets(Root.ToSharedRef(), TEXT("SGraphEditor"), GraphEditors);
	const UPackage* Package = Asset->GetPackage();
	TArray<FString> Available;
	for (const TSharedRef<SWidget>& Widget : GraphEditors)
	{
		const TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Widget);
		const UEdGraph* Graph = GraphEditor->GetCurrentGraph();
		if (!Graph || Graph->GetPackage() != Package)
		{
			continue;
		}

		TArray<UEdGraphNode*> Found;
		TArray<FString> Missing;
		for (const FString& Query : Nodes)
		{
			UEdGraphNode* Match = nullptr;
			for (UEdGraphNode* Candidate : Graph->Nodes)
			{
				if (Candidate && IluAssetEditor::Matches(Candidate, Query))
				{
					Match = Candidate;
					break;
				}
			}
			if (Match)
			{
				Found.AddUnique(Match);
			}
			else
			{
				Missing.Add(Query);
			}
		}
		if (!Missing.IsEmpty())
		{
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					Available.Add(IluAssetEditor::DescribeNode(Node));
				}
			}
			return FString::Printf(TEXT("Error: not found in graph %s: %s. Nodes (%d):\n%s"), *Graph->GetName(),
				*FString::Join(Missing, TEXT(", ")), Available.Num(), *FString::Join(Available, TEXT("\n")));
		}

		if (bFrame)
		{
			GraphEditor->JumpToNode(Found[0], /*bRequestRename*/ false, /*bSelectNode*/ false);
		}
		GraphEditor->ClearSelectionSet();
		TArray<FString> Selected;
		for (UEdGraphNode* Node : Found)
		{
			GraphEditor->SetNodeSelection(Node, true);
			Selected.Add(IluAssetEditor::DescribeNode(Node));
		}
		UE_LOG(LogIluUnrealTool, Log, TEXT("Agent: выделено нод в %s: %d."), *Asset->GetName(), Found.Num());
		return FString::Printf(TEXT("OK: selected %d node(s) in %s: %s"), Found.Num(), *Graph->GetName(),
			*FString::Join(Selected, TEXT(", ")));
	}
	return FString::Printf(TEXT("Error: the editor of %s shows no graph of this asset."), *Asset->GetName());
}

FString UIluAssetEditorToolset::CapturePreview(UObject* Asset, const FString& FilePath, float Yaw, float Pitch, float FOV)
{
	FString Error;
	IAssetEditorInstance* Editor = IluAssetEditor::FindEditor(Asset, Error);
	const TSharedPtr<SWidget> Root = Editor ? IluAssetEditor::GetEditorRoot(Editor, Error) : nullptr;
	if (!Root.IsValid())
	{
		return Error;
	}
	TSharedPtr<SWidget> ViewportWidget;
	FSceneViewport* Viewport = IluAssetEditor::FindPreviewViewport(Root.ToSharedRef(), ViewportWidget);
	if (!Viewport)
	{
		return FString::Printf(TEXT("Error: the editor of %s has no visible preview viewport."), *Asset->GetName());
	}
	FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(Viewport->GetClient());
	if (!Client->IsPerspective())
	{
		return TEXT("Error: the preview viewport is orthographic — switch it to Perspective.");
	}
	const FBox Box = IluAssetEditor::GetContentBounds(Client->GetWorld());
	if (!Box.IsValid)
	{
		return TEXT("Error: the preview is empty (nothing visible to frame) — select a node or wait for the preview to generate.");
	}

	// Камеру клиента ставим на время снимка и возвращаем: так свет, экспозиция и флаги показа — ровно как во вьюпорте.
	const FIntPoint Size = Viewport->GetSizeXY();
	const FVector OldLocation = Client->GetViewLocation();
	const FRotator OldRotation = Client->GetViewRotation();
	const FVector OldLookAt = Client->GetLookAtLocation();
	const float OldFOV = Client->ViewFOV;

	const float UsedFOV = FOV > 0.0f ? FMath::Clamp(FOV, 5.0f, 150.0f) : OldFOV;
	const double HalfFovX = FMath::DegreesToRadians(UsedFOV * 0.5);
	const double HalfFovY = FMath::Atan(FMath::Tan(HalfFovX) * Size.Y / Size.X);
	const FRotator Rotation(Pitch, Yaw, 0.0f);
	const FVector Center = Box.GetCenter();
	const FVector Location = Center - Rotation.Vector() * IluAssetEditor::FitDistance(Box, Rotation, HalfFovX, HalfFovY);

	Client->ViewFOV = UsedFOV;
	Client->SetViewLocation(Location);
	Client->SetViewRotation(Rotation);
	Client->SetLookAtLocation(Center); // орбитальная камера считает вид от точки взгляда — пусть совпадает с нашей
	for (int32 Pass = 0; Pass < 8; ++Pass) // история кадров: сглаживание, Lumen и экспозиция успевают сойтись
	{
		Viewport->Draw();
	}
	FlushRenderingCommands();
	TArray<FColor> Pixels;
	const bool bRead = Viewport->ReadPixels(Pixels);

	Client->ViewFOV = OldFOV;
	Client->SetViewLocation(OldLocation);
	Client->SetViewRotation(OldRotation);
	Client->SetLookAtLocation(OldLookAt);
	Client->Invalidate();

	if (!bRead || Pixels.Num() != Size.X * Size.Y)
	{
		return TEXT("Error: could not read the viewport image.");
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	const FString Path = FilePath.IsEmpty()
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("IluUnrealTool/Captures") / (Asset->GetName() + TEXT(".png")))
		: FilePath;
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	if (!FFileHelper::SaveArrayToFile(Png, *Path))
	{
		return FString::Printf(TEXT("Error: could not write %s."), *Path);
	}

	TArray<FString> Lines;
	if (ViewportWidget.IsValid())
	{
		IluAssetEditor::CollectTexts(ViewportWidget.ToSharedRef(), Lines);
	}
	const FVector Extent = Box.GetSize() / 100.0;
	UE_LOG(LogIluUnrealTool, Log, TEXT("Agent: снимок превью %s → %s."), *Asset->GetName(), *Path);
	return FString::Printf(TEXT("OK: saved %s (%dx%d, the viewport size), content %.2f x %.2f x %.2f m, camera yaw %.0f pitch %.0f fov %.0f\n"
		"=== Viewport text ===\n%s"), *Path, Size.X, Size.Y, Extent.X, Extent.Y, Extent.Z, Yaw, Pitch, UsedFOV,
		*FString::Join(Lines, TEXT("\n")));
}

FString UIluAssetEditorToolset::ExecuteEditorCommand(UObject* Asset, const FString& CommandContext, const FString& CommandName)
{
	FString Error;
	IAssetEditorInstance* Editor = IluAssetEditor::FindEditor(Asset, Error);
	if (!Editor)
	{
		return Error;
	}
	const TSharedPtr<FUICommandInfo> Command = FInputBindingManager::Get().FindCommandInContext(FName(*CommandContext), FName(*CommandName));
	if (!Command.IsValid())
	{
		return FString::Printf(TEXT("Error: no command %s in context %s."), *CommandName, *CommandContext);
	}
	// Все редакторы ассетов, кроме редактора уровня, — FAssetEditorToolkit; команды сидят в его списке.
	const FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Editor);
	if (!Toolkit->GetToolkitCommands()->TryExecuteAction(Command.ToSharedRef()))
	{
		return FString::Printf(TEXT("Error: the editor of %s does not handle %s.%s or it cannot run now."), *Asset->GetName(),
			*CommandContext, *CommandName);
	}
	return FString::Printf(TEXT("OK: ran %s.%s in the editor of %s."), *CommandContext, *CommandName, *Asset->GetName());
}

FString UIluAssetEditorToolset::CloseAssetEditor(UObject* Asset)
{
	if (!Asset)
	{
		return TEXT("Error: Asset is null.");
	}
	UAssetEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return TEXT("Error: no asset editor subsystem.");
	}
	const int32 Closed = Subsystem->CloseAllEditorsForAsset(Asset);
	return FString::Printf(TEXT("OK: closed %d editor(s) of %s%s."), Closed, *Asset->GetName(),
		Asset->GetPackage()->IsDirty() ? TEXT(" (the asset has unsaved changes)") : TEXT(""));
}
