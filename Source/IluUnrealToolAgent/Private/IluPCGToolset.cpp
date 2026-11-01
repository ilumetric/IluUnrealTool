#include "IluPCGToolset.h"
#include "IluUnrealToolCore.h"

#include "Algo/Transform.h"
#include "ScopedTransaction.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "IluPCGToolset"

namespace IluPCGHLSL
{
	// Плагин PCG не подключаем: классы и свойства ищем по именам. Код ноды (ShaderSource/ShaderFunctions) — UPROPERTY() без
	// EditAnywhere, поэтому штатный ObjectTools его не видит; редактор пишет его так же — PreEditChange/Modify/PostEditChangeProperty.
	static const FName CustomHLSLSettingsClass(TEXT("PCGCustomHLSLSettings"));
	static const FName ComputeSourceClass(TEXT("PCGComputeSource"));

	enum class EKind
	{
		None,
		CustomHLSL,
		ComputeSource,
	};

	static bool IsA(const UObject* Object, FName ClassName)
	{
		for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetFName() == ClassName)
			{
				return true;
			}
		}
		return false;
	}

	static EKind GetKind(const UObject* Object)
	{
		if (!Object)
		{
			return EKind::None;
		}
		if (IsA(Object, CustomHLSLSettingsClass))
		{
			return EKind::CustomHLSL;
		}
		if (IsA(Object, ComputeSourceClass))
		{
			return EKind::ComputeSource;
		}
		return EKind::None;
	}

	static UObject* GetObjectValue(UObject* Object, FName PropertyName)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}

	// Нода графа → её настройки: UPCGNode::SettingsInterface, а у UPCGSettingsInstance — ещё UPCGSettingsInstance::Settings.
	static UObject* Resolve(UObject* Target, EKind& OutKind)
	{
		UObject* Object = Target;
		OutKind = GetKind(Object);
		if (OutKind == EKind::None && Object)
		{
			if (UObject* SettingsInterface = GetObjectValue(Object, TEXT("SettingsInterface")))
			{
				Object = SettingsInterface;
				OutKind = GetKind(Object);
			}
		}
		if (OutKind == EKind::None && Object)
		{
			if (UObject* Settings = GetObjectValue(Object, TEXT("Settings")))
			{
				Object = Settings;
				OutKind = GetKind(Object);
			}
		}
		return OutKind == EKind::None ? nullptr : Object;
	}

	static FStrProperty* FindString(const UObject* Object, FName PropertyName)
	{
		return FindFProperty<FStrProperty>(Object->GetClass(), PropertyName);
	}

	static FString GetString(const UObject* Object, FName PropertyName)
	{
		const FStrProperty* Property = FindString(Object, PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : FString();
	}

	static FString DescribeAdditionalSources(UObject* Settings)
	{
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(Settings->GetClass(), TEXT("AdditionalSources"));
		const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (!Inner)
		{
			return TEXT("none");
		}

		TArray<FString> Paths;
		FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Settings));
		for (int32 Index = 0; Index < Array.Num(); ++Index)
		{
			if (const UObject* Source = Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)))
			{
				Paths.Add(Source->GetPathName());
			}
		}
		return Paths.IsEmpty() ? TEXT("none") : FString::Join(Paths, TEXT(", "));
	}

	static FString UnsupportedTargetError(const UObject* Target)
	{
		return FString::Printf(TEXT("Error: %s (%s) is not a Custom HLSL node, its settings or a PCG Compute Source asset."),
			*GetPathNameSafe(Target), Target ? *Target->GetClass()->GetName() : TEXT("null"));
	}

	// Нода с Kernel Source Override берёт код из ассета Compute Source, свой код у неё не используется (как IsReadOnly в редакторе).
	static UObject* GetKernelSourceOverride(UObject* Settings)
	{
		return GetObjectValue(Settings, TEXT("KernelSourceOverride"));
	}

	static FString SetString(UObject* Object, FName PropertyName, const FString& Value, const FText& TransactionName)
	{
		FStrProperty* Property = FindString(Object, PropertyName);
		if (!Property)
		{
			return FString::Printf(TEXT("Error: %s has no %s property (engine version changed?)."), *Object->GetClass()->GetName(), *PropertyName.ToString());
		}

		// Как SetSourceText в движке: базовый UPCGSettings::PostEditChangeProperty рассылает изменение (ShaderSource | Structural),
		// граф перекомпилируется; UPCGComputeSource::PostEditChangeProperty оповещает ноды, которые на него ссылаются.
		{
			FScopedTransaction Transaction(TransactionName);
			Object->PreEditChange(Property);
			Object->Modify();
			Property->SetPropertyValue_InContainer(Object, Value);
			FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
			Object->PostEditChangeProperty(Event);
		}

		UE_LOG(LogIluUnrealTool, Log, TEXT("%s: записан %s (%d симв.)."), *Object->GetPathName(), *PropertyName.ToString(), Value.Len());
		return FString::Printf(TEXT("OK: %s set (%d chars) on %s. Run the graph (PCGToolset.ExecuteGraphInstance) to see shader errors, then save the asset."),
			*PropertyName.ToString(), Value.Len(), *Object->GetPathName());
	}
}

namespace IluPCGNodes
{
	// Штатный PCGToolset ставит только ноды пакета /Script/PCG (по заголовку). Здесь — любые наследники UPCGSettings;
	// граф правим его же UFUNCTION (AddNodeOfType, ForceNotificationForEditor) через ProcessEvent, без модуля PCG.
	static const TCHAR* StockPackageName = TEXT("/Script/PCG");

	static UClass* FindPCGClass(const TCHAR* ClassName)
	{
		return FindObject<UClass>(nullptr, *FString::Printf(TEXT("%s.%s"), StockPackageName, ClassName));
	}

	// Как палитра нод редактора: абстрактные, устаревшие и скрытые классы и настройки с bExposeToLibrary = false не ставим.
	static bool IsPlaceable(UClass* Class)
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden))
		{
			return false;
		}
		const FBoolProperty* ExposeToLibrary = FindFProperty<FBoolProperty>(Class, TEXT("bExposeToLibrary"));
		return !ExposeToLibrary || ExposeToLibrary->GetPropertyValue_InContainer(Class->GetDefaultObject());
	}

	static TArray<UClass*> GetNodeClasses(UClass* SettingsClass)
	{
		TArray<UClass*> Classes;
		GetDerivedClasses(SettingsClass, Classes, /*bRecursive=*/true);
		Classes.RemoveAll([](UClass* Class) { return !IsPlaceable(Class); });
		Classes.Sort([](const UClass& A, const UClass& B) { return A.GetPathName() < B.GetPathName(); });
		return Classes;
	}

	// Путь класса, имя класса (с префиксом U или без) или отображаемое имя; путь важнее остального.
	static UClass* ResolveNodeClass(UClass* SettingsClass, const FString& NodeType, FString& OutError)
	{
		const FString Type = NodeType.TrimStartAndEnd();
		TArray<UClass*> Matches;
		for (UClass* Class : GetNodeClasses(SettingsClass))
		{
			if (Class->GetPathName().Equals(Type, ESearchCase::IgnoreCase))
			{
				return Class;
			}
			if (Class->GetName().Equals(Type, ESearchCase::IgnoreCase) || (TEXT("U") + Class->GetName()).Equals(Type, ESearchCase::IgnoreCase)
				|| Class->GetDisplayNameText().ToString().Equals(Type, ESearchCase::IgnoreCase))
			{
				Matches.Add(Class);
			}
		}

		if (Matches.Num() == 1)
		{
			return Matches[0];
		}

		TArray<FString> Paths;
		Algo::Transform(Matches, Paths, [](const UClass* Class) { return Class->GetPathName(); });
		OutError = Matches.IsEmpty()
			? FString::Printf(TEXT("Error: no placeable PCG node type '%s'; find its class path with ListNodeTypes."), *Type)
			: FString::Printf(TEXT("Error: '%s' is ambiguous, pass the class path: %s."), *Type, *FString::Join(Paths, TEXT(", ")));
		return nullptr;
	}

	template <typename TProperty, typename TValue>
	static void SetValue(UObject* Object, FName PropertyName, const TValue& Value)
	{
		if (TProperty* Property = FindFProperty<TProperty>(Object->GetClass(), PropertyName))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
	}

	// Подписи пинов ноды: UPCGNode::InputPins / OutputPins → UPCGPin::Properties.Label.
	static FString DescribePins(UObject* Node, FName ArrayName)
	{
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(Node->GetClass(), ArrayName);
		const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (!Inner)
		{
			return TEXT("?");
		}

		TArray<FString> Labels;
		FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Node));
		for (int32 Index = 0; Index < Array.Num(); ++Index)
		{
			const UObject* Pin = Inner->GetObjectPropertyValue(Array.GetRawPtr(Index));
			const FStructProperty* PinProperties = Pin ? FindFProperty<FStructProperty>(Pin->GetClass(), TEXT("Properties")) : nullptr;
			const FNameProperty* Label = PinProperties ? FindFProperty<FNameProperty>(PinProperties->Struct, TEXT("Label")) : nullptr;
			if (Label)
			{
				Labels.Add(Label->GetPropertyValue_InContainer(PinProperties->ContainerPtrToValuePtr<void>(Pin)).ToString());
			}
		}
		return Labels.IsEmpty() ? TEXT("none") : FString::Join(Labels, TEXT(", "));
	}

	// UPCGGraph::AddNodeOfType(TSubclassOf<UPCGSettings>, UPCGSettings*&) — BlueprintCallable, зовём через ProcessEvent.
	static UObject* CallAddNodeOfType(UObject* Graph, UClass* NodeClass)
	{
		UFunction* Function = Graph->FindFunction(TEXT("AddNodeOfType"));
		const FClassProperty* ClassParam = Function ? CastField<FClassProperty>(Function->FindPropertyByName(TEXT("InSettingsClass"))) : nullptr;
		const FObjectPropertyBase* ReturnParam = Function ? CastField<FObjectPropertyBase>(Function->GetReturnProperty()) : nullptr;
		if (!ClassParam || !ReturnParam)
		{
			return nullptr;
		}

		FStructOnScope Params(Function);
		ClassParam->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), NodeClass);
		Graph->ProcessEvent(Function, Params.GetStructMemory());
		return ReturnParam->GetObjectPropertyValue_InContainer(Params.GetStructMemory());
	}

	// UPCGGraph::ForceNotificationForEditor(EPCGChangeType::Structural) — открытый редактор графа перестраивается.
	static void NotifyEditor(UObject* Graph)
	{
		UFunction* Function = Graph->FindFunction(TEXT("ForceNotificationForEditor"));
		const FEnumProperty* ChangeParam = Function ? CastField<FEnumProperty>(Function->FindPropertyByName(TEXT("ChangeType"))) : nullptr;
		if (!ChangeParam)
		{
			return;
		}

		FStructOnScope Params(Function);
		const UEnum* ChangeType = ChangeParam->GetEnum();
		for (int32 Index = 0; Index < ChangeType->NumEnums(); ++Index)
		{
			if (ChangeType->GetNameStringByIndex(Index) == TEXT("Structural"))
			{
				ChangeParam->GetUnderlyingProperty()->SetIntPropertyValue(ChangeParam->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),
					ChangeType->GetValueByIndex(Index));
				break;
			}
		}
		Graph->ProcessEvent(Function, Params.GetStructMemory());
	}
}

FString UIluPCGToolset::GetHLSLSource(UObject* Target, bool bIncludeDeclarations)
{
	using namespace IluPCGHLSL;

	EKind Kind = EKind::None;
	UObject* Object = Resolve(Target, Kind);
	if (!Object)
	{
		return UnsupportedTargetError(Target);
	}

	if (Kind == EKind::ComputeSource)
	{
		return FString::Printf(TEXT("OK: PCG Compute Source %s\n=== Source ===\n%s"), *Object->GetPathName(), *GetString(Object, TEXT("Source")));
	}

	const UObject* Override = GetKernelSourceOverride(Object);
	FString Result = FString::Printf(TEXT("OK: Custom HLSL settings %s\n"), *Object->GetPathName());
	Result += FString::Printf(TEXT("Kernel source override: %s\n"),
		Override ? *FString::Printf(TEXT("%s (the node runs that asset's code; edit the asset instead)"), *Override->GetPathName()) : TEXT("none"));
	Result += FString::Printf(TEXT("Additional sources: %s\n"), *DescribeAdditionalSources(Object));
	Result += FString::Printf(TEXT("=== Source ===\n%s\n"), *GetString(Object, TEXT("ShaderSource")));
	Result += FString::Printf(TEXT("=== Functions ===\n%s\n"), *GetString(Object, TEXT("ShaderFunctions")));
	if (bIncludeDeclarations)
	{
		// Объявления транзиентные: нода пересчитывает их при загрузке и после каждой правки (UpdateDeclarations).
		Result += FString::Printf(TEXT("=== Declarations ===\n%s\n\n%s\n\n%s\n"),
			*GetString(Object, TEXT("InputDeclarations")), *GetString(Object, TEXT("OutputDeclarations")), *GetString(Object, TEXT("HelperDeclarations")));
	}
	return Result;
}

FString UIluPCGToolset::SetHLSLSource(UObject* Target, const FString& Source)
{
	using namespace IluPCGHLSL;

	EKind Kind = EKind::None;
	UObject* Object = Resolve(Target, Kind);
	if (!Object)
	{
		return UnsupportedTargetError(Target);
	}

	if (Kind == EKind::ComputeSource)
	{
		return SetString(Object, TEXT("Source"), Source, LOCTEXT("SetComputeSource", "Set Compute Source"));
	}

	if (const UObject* Override = GetKernelSourceOverride(Object))
	{
		return FString::Printf(TEXT("Error: the node uses Kernel Source Override %s; edit that asset (SetHLSLSource on it)."), *Override->GetPathName());
	}
	return SetString(Object, TEXT("ShaderSource"), Source, LOCTEXT("SetShaderSource", "Set Source Text"));
}

FString UIluPCGToolset::SetHLSLFunctions(UObject* Target, const FString& Functions)
{
	using namespace IluPCGHLSL;

	EKind Kind = EKind::None;
	UObject* Object = Resolve(Target, Kind);
	if (Kind != EKind::CustomHLSL)
	{
		return Kind == EKind::ComputeSource
			? FString(TEXT("Error: a PCG Compute Source has no functions block; put the functions into its Source."))
			: UnsupportedTargetError(Target);
	}

	if (const UObject* Override = GetKernelSourceOverride(Object))
	{
		return FString::Printf(TEXT("Error: the node uses Kernel Source Override %s; edit that asset (SetHLSLSource on it)."), *Override->GetPathName());
	}
	return SetString(Object, TEXT("ShaderFunctions"), Functions, LOCTEXT("SetShaderFunctions", "Set Functions Text"));
}

FString UIluPCGToolset::ListNodeTypes(const FString& Filter)
{
	using namespace IluPCGNodes;

	UClass* SettingsClass = FindPCGClass(TEXT("PCGSettings"));
	if (!SettingsClass)
	{
		return TEXT("Error: the PCG plugin is not loaded.");
	}

	const FString Needle = Filter.TrimStartAndEnd();
	TArray<FString> Lines;
	for (UClass* Class : GetNodeClasses(SettingsClass))
	{
		const FString DisplayName = Class->GetDisplayNameText().ToString();
		const FString Path = Class->GetPathName();
		if (!Needle.IsEmpty() && !DisplayName.Contains(Needle) && !Path.Contains(Needle))
		{
			continue;
		}
		const bool bStock = Class->GetPackage()->GetName() == StockPackageName;
		Lines.Add(FString::Printf(TEXT("%s | %s%s"), *DisplayName, *Path, bStock ? TEXT(" [stock]") : TEXT("")));
	}

	return FString::Printf(TEXT("OK: %d node types (display name | class path; [stock] = also placeable with PCGToolset.AddNode by node title)\n%s"),
		Lines.Num(), *FString::Join(Lines, TEXT("\n")));
}

FString UIluPCGToolset::AddNodeByClass(UObject* Graph, const FString& NodeType, const FString& NodeName, const FString& NodeTitle,
	int32 XPosition, int32 YPosition)
{
	using namespace IluPCGNodes;

	UClass* SettingsClass = FindPCGClass(TEXT("PCGSettings"));
	if (!SettingsClass)
	{
		return TEXT("Error: the PCG plugin is not loaded.");
	}
	if (!Graph || !IluPCGHLSL::IsA(Graph, TEXT("PCGGraph")))
	{
		return FString::Printf(TEXT("Error: %s is not a PCG graph."), *GetPathNameSafe(Graph));
	}

	FString Error;
	UClass* NodeClass = ResolveNodeClass(SettingsClass, NodeType, Error);
	if (!NodeClass)
	{
		return Error;
	}

	// Имя проверяем до добавления: отменённая транзакция ноду из графа не уберёт.
	const FString Name = NodeName.TrimStartAndEnd();
	if (!Name.IsEmpty())
	{
		FText Reason;
		if (!FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &Reason))
		{
			return FString::Printf(TEXT("Error: invalid node name '%s': %s"), *Name, *Reason.ToString());
		}
		if (StaticFindObjectFast(nullptr, Graph, FName(*Name)))
		{
			return FString::Printf(TEXT("Error: the graph already has an object named '%s'."), *Name);
		}
	}

	UObject* Node = nullptr;
	{
		FScopedTransaction Transaction(LOCTEXT("AddNodeByClass", "Add PCG Node"));
		Graph->Modify();
		Node = CallAddNodeOfType(Graph, NodeClass);
		if (!Node)
		{
			Transaction.Cancel();
			return FString::Printf(TEXT("Error: UPCGGraph::AddNodeOfType failed for %s (engine version changed?)."), *NodeClass->GetPathName());
		}

		if (!Name.IsEmpty())
		{
			Node->Rename(*Name, nullptr, REN_DontCreateRedirectors);
		}
		if (!NodeTitle.IsEmpty())
		{
			SetValue<FNameProperty>(Node, TEXT("NodeTitle"), FName(*NodeTitle));
		}
		SetValue<FIntProperty>(Node, TEXT("PositionX"), XPosition);
		SetValue<FIntProperty>(Node, TEXT("PositionY"), YPosition);
	}

	(void)Graph->MarkPackageDirty();
	NotifyEditor(Graph);

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: добавлена нода %s (%s)."), *Graph->GetPathName(), *Node->GetName(), *NodeClass->GetName());
	return FString::Printf(TEXT("OK: added %s node %s. Input pins: %s. Output pins: %s. Set parameters with PCGToolset.UpdateNode, wire with PCGToolset.ConnectNodePins, then save the graph."),
		*NodeClass->GetName(), *Node->GetPathName(), *DescribePins(Node, TEXT("InputPins")), *DescribePins(Node, TEXT("OutputPins")));
}

#undef LOCTEXT_NAMESPACE
