#include "IluMeshOpsMenu.h"
#include "IluEditorNotify.h"
#include "IluMeshBake.h"
#include "IluMeshOperations.h"
#include "IluUnrealToolCore.h"

#include "Components/StaticMeshComponent.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "IluMeshOps"

namespace IluMeshOpsMenu
{
	static const TCHAR* SettingsSection = TEXT("IluUnrealTool.MeshOps");
	static int32 GAORays = 64;
	static float GAODistance = 50.f;
	static bool GExportPivotBottom = true;
	static FString GExportDir;
	static bool GSettingsLoaded = false;

	static void LoadSettings()
	{
		if (GSettingsLoaded || !GConfig)
		{
			return;
		}
		GSettingsLoaded = true;
		GConfig->GetInt(SettingsSection, TEXT("AORays"), GAORays, GEditorPerProjectIni);
		GConfig->GetFloat(SettingsSection, TEXT("AODistance"), GAODistance, GEditorPerProjectIni);
		GConfig->GetBool(SettingsSection, TEXT("ExportPivotBottom"), GExportPivotBottom, GEditorPerProjectIni);
		GConfig->GetString(SettingsSection, TEXT("ExportDir"), GExportDir, GEditorPerProjectIni);
		GAORays = FMath::Clamp(GAORays, FIluMeshOpsMenu::AORaysMin, FIluMeshOpsMenu::AORaysMax);
		GAODistance = FMath::Clamp(GAODistance, 1.f, FIluMeshOpsMenu::AODistanceMax);
	}

	/** Уникальные статик-меши проекта (/Game) у выделенных акторов; движковые и плагинные не трогаем. */
	static TArray<UStaticMesh*> CollectSelectedMeshes()
	{
		TArray<UStaticMesh*> Meshes;
		if (!GEditor)
		{
			return Meshes;
		}
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			const AActor* Actor = Cast<AActor>(*It);
			if (!Actor)
			{
				continue;
			}
			TArray<UStaticMeshComponent*> Components;
			Actor->GetComponents(Components);
			for (const UStaticMeshComponent* Component : Components)
			{
				UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
				if (Mesh && Mesh->GetPathName().StartsWith(TEXT("/Game/")))
				{
					Meshes.AddUnique(Mesh);
				}
			}
		}
		return Meshes;
	}

	/** Общий прогон: подтверждение → операция над каждым мешем → сводка. Ассеты помечаются изменёнными, сохраняет человек. */
	static void Run(const FText& OpName, TFunctionRef<FString(UStaticMesh*)> Op)
	{
		FSlateApplication::Get().DismissAllMenus();

		const TArray<UStaticMesh*> Meshes = CollectSelectedMeshes();
		if (Meshes.Num() == 0)
		{
			FIluEditorNotify::Show(FText::Format(LOCTEXT("NoMeshes", "{0}: среди выделенных акторов нет статик-мешей проекта"), OpName), false);
			return;
		}

		const FText Question = FText::Format(LOCTEXT("Confirm",
			"{0}: изменить ассеты мешей — {1} шт.?\nМеняется сам ассет (все его экземпляры на всех уровнях). Отмены (Ctrl+Z) нет — откат через систему контроля версий."),
			OpName, FText::AsNumber(Meshes.Num()));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
		{
			return;
		}

		FScopedSlowTask Task(Meshes.Num(), OpName);
		Task.MakeDialog();
		int32 Failed = 0;
		for (UStaticMesh* Mesh : Meshes)
		{
			Task.EnterProgressFrame(1.f, FText::FromString(Mesh->GetName()));
			const FString Result = Op(Mesh);
			if (!FIluMeshOperations::IsSuccess(Result))
			{
				++Failed;
				UE_LOG(LogIluUnrealTool, Warning, TEXT("%s — %s: %s"), *OpName.ToString(), *Mesh->GetName(), *Result);
			}
		}

		FIluEditorNotify::Show(FText::Format(LOCTEXT("Done", "{0}: готово, мешей {1}, ошибок {2}. Не забудь сохранить ассеты."),
			OpName, FText::AsNumber(Meshes.Num()), FText::AsNumber(Failed)), Failed == 0, 5.f);
	}

	/** Выделенные акторы -> окно «Сохранить как» -> один общий меш в .fbx/.obj. В Content ничего не создаётся. */
	static void ExportSelected()
	{
		FSlateApplication::Get().DismissAllMenus();
		LoadSettings();

		TArray<AActor*> Actors;
		if (GEditor)
		{
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					Actors.Add(Actor);
				}
			}
		}
		if (Actors.Num() == 0)
		{
			FIluEditorNotify::Show(LOCTEXT("ExportNoActors", "Экспорт мешей: ничего не выделено"), false);
			return;
		}

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			return;
		}
		const FString DefaultDir = GExportDir.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) : GExportDir;
		TArray<FString> Files;
		const bool bPicked = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Export selected meshes / Экспорт выделенных мешей"),
			DefaultDir,
			Actors[0]->GetActorLabel() + TEXT(".fbx"),
			TEXT("FBX (*.fbx)|*.fbx|OBJ (*.obj)|*.obj"),
			EFileDialogFlags::None,
			Files);
		if (!bPicked || Files.Num() == 0)
		{
			return;
		}
		const FString FilePath = FPaths::ConvertRelativePathToFull(Files[0]);

		GExportDir = FPaths::GetPath(FilePath);
		if (GConfig)
		{
			GConfig->SetString(SettingsSection, TEXT("ExportDir"), *GExportDir, GEditorPerProjectIni);
		}

		FString Result;
		{
			FScopedSlowTask Task(1.f, LOCTEXT("ExportProgress", "Экспорт мешей..."));
			Task.MakeDialog();
			Task.EnterProgressFrame(1.f);
			Result = FIluMeshBake::ExportActorsToFile(Actors, FilePath, GExportPivotBottom);
		}

		const bool bOk = FIluMeshOperations::IsSuccess(Result);
		if (!bOk)
		{
			UE_LOG(LogIluUnrealTool, Warning, TEXT("Экспорт мешей: %s"), *Result);
		}
		FIluEditorNotify::Show(bOk
			? FText::Format(LOCTEXT("ExportDone", "Экспорт мешей: {0}"), FText::FromString(FilePath))
			: FText::FromString(Result), bOk, 5.f);
	}
}

int32 FIluMeshOpsMenu::GetAORays()
{
	IluMeshOpsMenu::LoadSettings();
	return IluMeshOpsMenu::GAORays;
}

void FIluMeshOpsMenu::SetAORays(int32 Rays)
{
	IluMeshOpsMenu::LoadSettings();
	IluMeshOpsMenu::GAORays = FMath::Clamp(Rays, AORaysMin, AORaysMax);
	if (GConfig)
	{
		GConfig->SetInt(IluMeshOpsMenu::SettingsSection, TEXT("AORays"), IluMeshOpsMenu::GAORays, GEditorPerProjectIni);
	}
}

float FIluMeshOpsMenu::GetAODistance()
{
	IluMeshOpsMenu::LoadSettings();
	return IluMeshOpsMenu::GAODistance;
}

void FIluMeshOpsMenu::SetAODistance(float Cm)
{
	IluMeshOpsMenu::LoadSettings();
	IluMeshOpsMenu::GAODistance = FMath::Clamp(Cm, 1.f, AODistanceMax);
	if (GConfig)
	{
		GConfig->SetFloat(IluMeshOpsMenu::SettingsSection, TEXT("AODistance"), IluMeshOpsMenu::GAODistance, GEditorPerProjectIni);
	}
}

void FIluMeshOpsMenu::AddSection(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("IluMeshOps", LOCTEXT("MeshOpsSection", "Mesh Ops (selected actors)"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MergeSlotsLabel", "Merge material slots"),
		LOCTEXT("MergeSlotsTooltip",
			"Static meshes of the selected actors: move all geometry into the first material slot and delete the other slots.\n"
			"Статик-меши выделенных акторов: вся геометрия — в первый слот материала, остальные слоты удаляются."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			IluMeshOpsMenu::Run(LOCTEXT("MergeSlotsOp", "Слить слоты материалов"),
				[](UStaticMesh* Mesh) { return FIluMeshOperations::MergeMaterialSlots(Mesh, 0); });
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearColorLabel", "Clear vertex color (0,0,0,0)"),
		LOCTEXT("ClearColorTooltip",
			"Static meshes of the selected actors: set the vertex color of all vertices to RGBA 0,0,0,0.\n"
			"Статик-меши выделенных акторов: вертекс-колор всех вершин = RGBA 0,0,0,0."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			IluMeshOpsMenu::Run(LOCTEXT("ClearColorOp", "Обнулить вертекс-колор"),
				[](UStaticMesh* Mesh) { return FIluMeshOperations::FillVertexColor(Mesh, FLinearColor(0.f, 0.f, 0.f, 0.f)); });
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("KeepUV0Label", "Keep only UV0"),
		LOCTEXT("KeepUV0Tooltip",
			"Static meshes of the selected actors: delete every UV channel except UV0, turn off Generate Lightmap UVs and point the lightmap at UV0.\n"
			"Статик-меши выделенных акторов: удалить все UV-каналы, кроме UV0, выключить Generate Lightmap UVs и перевести лайтмап на UV0."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			IluMeshOpsMenu::Run(LOCTEXT("KeepUV0Op", "Оставить только UV0"),
				[](UStaticMesh* Mesh) { return FIluMeshOperations::KeepOnlyUV0(Mesh); });
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveLodsLabel", "Remove LODs (keep LOD0)"),
		LOCTEXT("RemoveLodsTooltip",
			"Static meshes of the selected actors: delete all LODs except LOD0, imported ones included (for Nanite meshes).\n"
			"Статик-меши выделенных акторов: удалить все LOD, кроме LOD0, включая импортные (для Nanite-мешей)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			IluMeshOpsMenu::Run(LOCTEXT("RemoveLodsOp", "Удалить LOD"),
				[](UStaticMesh* Mesh) { return FIluMeshOperations::RemoveLods(Mesh); });
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CollisionDefaultLabel", "Collision: Project Default"),
		LOCTEXT("CollisionDefaultTooltip",
			"Static meshes of the selected actors: set Collision Complexity to Project Default (e.g. instead of Use Complex As Simple).\n"
			"Статик-меши выделенных акторов: Collision Complexity = Project Default (например, вместо Use Complex As Simple)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			IluMeshOpsMenu::Run(LOCTEXT("CollisionDefaultOp", "Коллизия: Project Default"),
				[](UStaticMesh* Mesh) { return FIluMeshOperations::SetCollisionProjectDefault(Mesh); });
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BakeAOLabel", "Bake AO to vertex alpha"),
		LOCTEXT("BakeAOTooltip",
			"Static meshes of the selected actors: bake ambient occlusion (from the mesh itself) into the vertex color alpha; RGB is kept.\n"
			"Статик-меши выделенных акторов: запечь ambient occlusion (от самого меша) в альфу вертекс-колора; RGB не трогается."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			const int32 Rays = GetAORays();
			const float Distance = GetAODistance();
			IluMeshOpsMenu::Run(LOCTEXT("BakeAOOp", "Запечь AO в альфу"),
				[Rays, Distance](UStaticMesh* Mesh) { return FIluMeshOperations::BakeAOToVertexAlpha(Mesh, Rays, Distance); });
		})));

	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(100.f)
		[
			SNew(SSpinBox<int32>)
			.MinValue(AORaysMin)
			.MaxValue(AORaysMax)
			.Delta(16)
			.ToolTipText(LOCTEXT("AORaysTooltip",
				"Rays per vertex for the AO bake: more — smoother, slower.\n"
				"Лучей на вершину при запекании AO: больше — ровнее, но дольше."))
			.Value_Static(&FIluMeshOpsMenu::GetAORays)
			.OnValueChanged_Static(&FIluMeshOpsMenu::SetAORays)
		],
		LOCTEXT("AORaysLabel", "AO rays"));

	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(100.f)
		[
			SNew(SSpinBox<float>)
			.MinValue(1.f)
			.MaxValue(AODistanceMax)
			.Delta(5.f)
			.ToolTipText(LOCTEXT("AODistanceTooltip",
				"AO search distance, cm: occluders farther than this do not darken.\n"
				"Дистанция поиска AO, см: дальше неё препятствия не затемняют."))
			.Value_Static(&FIluMeshOpsMenu::GetAODistance)
			.OnValueChanged_Static(&FIluMeshOpsMenu::SetAODistance)
		],
		LOCTEXT("AODistanceLabel", "AO distance (cm)"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportLabel", "Export selected to FBX/OBJ..."),
		LOCTEXT("ExportTooltip",
			"Merge the visible meshes of the selected actors (Dynamic Mesh blockouts and Static Mesh) into one mesh and save it to disk as .fbx or .obj.\n"
			"Слить видимые меши выделенных акторов (блокауты Dynamic Mesh и Static Mesh) в один меш и сохранить на диск в .fbx или .obj."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&IluMeshOpsMenu::ExportSelected)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportPivotLabel", "Export: pivot at bottom center"),
		LOCTEXT("ExportPivotTooltip",
			"On: the exported mesh pivot is at the bottom center of the selection. Off: at the world origin (keeps level placement).\n"
			"Вкл.: пивот экспортируемого меша — в центре низа выделения. Выкл.: в начале координат мира (сохраняет положение на уровне)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				IluMeshOpsMenu::LoadSettings();
				IluMeshOpsMenu::GExportPivotBottom = !IluMeshOpsMenu::GExportPivotBottom;
				if (GConfig)
				{
					GConfig->SetBool(IluMeshOpsMenu::SettingsSection, TEXT("ExportPivotBottom"), IluMeshOpsMenu::GExportPivotBottom, GEditorPerProjectIni);
				}
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { IluMeshOpsMenu::LoadSettings(); return IluMeshOpsMenu::GExportPivotBottom; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
