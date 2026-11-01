#include "IluPCGToolset.h"
#include "IluUnrealToolCore.h"

#include "ScopedTransaction.h"
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

#undef LOCTEXT_NAMESPACE
