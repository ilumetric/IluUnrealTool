#include "IluBlueprintToolset.h"
#include "IluUnrealToolCore.h"

#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/UserDefinedEnum.h"
#include "Factories/EnumFactory.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"

namespace IluBlueprintEnum
{
	/** Entries must be non-empty and unique. / Элементы — непустые и уникальные. */
	static bool ValidateEntries(const TArray<FString>& Entries, FString& OutError)
	{
		if (Entries.IsEmpty())
		{
			OutError = TEXT("Error: Entries is empty.");
			return false;
		}
		if (Entries.Num() > 255)
		{
			OutError = TEXT("Error: a Blueprint enum holds at most 255 entries.");
			return false;
		}
		TSet<FString> Seen;
		for (const FString& Entry : Entries)
		{
			if (Entry.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("Error: an entry is empty.");
				return false;
			}
			bool bAlreadySeen = false;
			Seen.Add(Entry, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutError = FString::Printf(TEXT("Error: duplicate entry '%s'."), *Entry);
				return false;
			}
		}
		return true;
	}

	/** Number of entries without the hidden _MAX. / Число элементов без скрытого _MAX. */
	static int32 NumEntries(const UEnum* Enum)
	{
		return Enum->NumEnums() - (Enum->ContainsExistingMax() ? 1 : 0);
	}

	static FString Describe(const UEnum* Enum)
	{
		TArray<FString> Parts;
		for (int32 Index = 0; Index < NumEntries(Enum); ++Index)
		{
			Parts.Add(FString::Printf(TEXT("%d=%s (%s)"), Index,
				*Enum->GetDisplayNameTextByIndex(Index).ToString(), *Enum->GetNameStringByIndex(Index)));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	static FString ApplyEntries(UUserDefinedEnum* Enum, const TArray<FString>& Entries)
	{
		while (NumEntries(Enum) < Entries.Num())
		{
			FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(Enum);
		}
		while (NumEntries(Enum) > Entries.Num())
		{
			FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(Enum, NumEntries(Enum) - 1);
		}

		// Сначала временные имена: иначе перестановка (A, B → B, A) упрётся в проверку уникальности.
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index, FText::FromString(FString::Printf(TEXT("__IluTmp_%d"), Index)));
		}
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			if (!FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index, FText::FromString(Entries[Index])))
			{
				return FString::Printf(TEXT("Error: the engine rejected display name '%s' (index %d)."), *Entries[Index], Index);
			}
		}

		Enum->MarkPackageDirty();
		return FString();
	}
}

namespace IluBlueprintVariable
{
	/** Turns a type string into a Blueprint pin type. / Строка типа → тип пина Blueprint. */
	static bool ResolvePinType(const FString& Type, FEdGraphPinType& OutType, FString& OutError)
	{
		const FString Key = Type.TrimStartAndEnd();
		const FString Lower = Key.ToLower();

		static const TMap<FString, FName> Primitives = {
			{ TEXT("bool"), UEdGraphSchema_K2::PC_Boolean },
			{ TEXT("byte"), UEdGraphSchema_K2::PC_Byte },
			{ TEXT("int"), UEdGraphSchema_K2::PC_Int },
			{ TEXT("int64"), UEdGraphSchema_K2::PC_Int64 },
			{ TEXT("name"), UEdGraphSchema_K2::PC_Name },
			{ TEXT("string"), UEdGraphSchema_K2::PC_String },
			{ TEXT("text"), UEdGraphSchema_K2::PC_Text },
		};
		if (const FName* Category = Primitives.Find(Lower))
		{
			OutType.PinCategory = *Category;
			return true;
		}
		if (Lower == TEXT("float") || Lower == TEXT("double") || Lower == TEXT("real"))
		{
			// В Blueprint UE5 float — это real/double.
			OutType.PinCategory = UEdGraphSchema_K2::PC_Real;
			OutType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
			return true;
		}

		// Путь без имени объекта (/Game/X/E_Y) дополняем до /Game/X/E_Y.E_Y.
		FString ObjectPath = Key;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			ObjectPath = ObjectPath + TEXT(".") + FPackageName::GetShortName(ObjectPath);
		}
		UObject* TypeObject = FSoftObjectPath(ObjectPath).TryLoad();
		if (const UBlueprint* TypeBlueprint = Cast<UBlueprint>(TypeObject))
		{
			TypeObject = TypeBlueprint->GeneratedClass;
		}

		if (const UEnum* Enum = Cast<UEnum>(TypeObject))
		{
			if (!UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Enum))
			{
				OutError = FString::Printf(TEXT("Error: enum %s is not a Blueprint type."), *Key);
				return false;
			}
			OutType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			OutType.PinSubCategoryObject = const_cast<UEnum*>(Enum);
			return true;
		}
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			if (!UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct))
			{
				OutError = FString::Printf(TEXT("Error: struct %s is not a Blueprint type."), *Key);
				return false;
			}
			OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			OutType.PinSubCategoryObject = const_cast<UScriptStruct*>(Struct);
			return true;
		}
		if (const UClass* Class = Cast<UClass>(TypeObject))
		{
			if (!UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Class))
			{
				OutError = FString::Printf(TEXT("Error: class %s is not a Blueprint type."), *Key);
				return false;
			}
			OutType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutType.PinSubCategoryObject = const_cast<UClass*>(Class);
			return true;
		}

		OutError = FString::Printf(TEXT("Error: unknown type '%s' (expected bool, byte, int, int64, float, name, string, text, ")
			TEXT("or a path to an enum, struct or class)."), *Key);
		return false;
	}
}

FString UIluBlueprintToolset::CreateEnum(const FString& AssetPath, const TArray<FString>& Entries)
{
	FString Error;
	if (!IluBlueprintEnum::ValidateEntries(Entries, Error))
	{
		return Error;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath.TrimStartAndEnd());
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
	{
		return FString::Printf(TEXT("Error: invalid asset path '%s': %s"), *AssetPath, *Reason.ToString());
	}
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		return FString::Printf(TEXT("Error: %s already exists. Use SetEnumEntries to change it."), *PackageName);
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UEnumFactory* Factory = NewObject<UEnumFactory>();
	UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(AssetTools.CreateAsset(
		FPackageName::GetShortName(PackageName), FPackageName::GetLongPackagePath(PackageName), UUserDefinedEnum::StaticClass(), Factory));
	if (!Enum)
	{
		return FString::Printf(TEXT("Error: the engine did not create %s."), *PackageName);
	}

	Error = IluBlueprintEnum::ApplyEntries(Enum, Entries);
	if (!Error.IsEmpty())
	{
		return Error;
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("Создан энум %s: %d элементов."), *PackageName, Entries.Num());
	return FString::Printf(TEXT("OK: created %s (not saved): %s"), *Enum->GetPathName(), *IluBlueprintEnum::Describe(Enum));
}

FString UIluBlueprintToolset::SetEnumEntries(UUserDefinedEnum* Enum, const TArray<FString>& Entries)
{
	if (!Enum)
	{
		return TEXT("Error: Enum is not a Blueprint enum (UserDefinedEnum).");
	}
	FString Error;
	if (!IluBlueprintEnum::ValidateEntries(Entries, Error))
	{
		return Error;
	}

	const int32 Before = IluBlueprintEnum::NumEntries(Enum);
	Error = IluBlueprintEnum::ApplyEntries(Enum, Entries);
	if (!Error.IsEmpty())
	{
		return Error;
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("Энум %s: элементов было %d, стало %d."), *Enum->GetPathName(), Before, Entries.Num());
	return FString::Printf(TEXT("OK: %s (was %d entries, not saved): %s"), *Enum->GetPathName(), Before, *IluBlueprintEnum::Describe(Enum));
}

FString UIluBlueprintToolset::GetEnumEntries(UEnum* Enum)
{
	if (!Enum)
	{
		return TEXT("Error: Enum is null.");
	}
	return FString::Printf(TEXT("OK: %s, %d entries: %s"), *Enum->GetPathName(), IluBlueprintEnum::NumEntries(Enum),
		*IluBlueprintEnum::Describe(Enum));
}

FString UIluBlueprintToolset::SetVariableType(UBlueprint* Blueprint, FName VariableName, const FString& Type)
{
	if (!Blueprint)
	{
		return TEXT("Error: Blueprint is null.");
	}
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
	if (VarIndex == INDEX_NONE)
	{
		return FString::Printf(TEXT("Error: %s has no member variable '%s' (inherited and local variables are not supported)."),
			*Blueprint->GetName(), *VariableName.ToString());
	}

	const FEdGraphPinType OldType = Blueprint->NewVariables[VarIndex].VarType;
	if (OldType.IsMap())
	{
		return TEXT("Error: map variables are not supported (the value type is ambiguous).");
	}

	FEdGraphPinType NewType;
	FString Error;
	if (!IluBlueprintVariable::ResolvePinType(Type, NewType, Error))
	{
		return Error;
	}
	NewType.ContainerType = OldType.ContainerType;
	if (OldType == NewType)
	{
		return FString::Printf(TEXT("OK: '%s' already has this type, nothing changed."), *VariableName.ToString());
	}

	// Если переменная уже стоит в графах, движок спрашивает подтверждение модальным окном — через MCP оно повесит редактор.
	// На время вызова помечаем диалог подавленным (как галка «не спрашивать»), потом возвращаем как было.
	const TCHAR* DialogSection = TEXT("SuppressableDialogs");
	const TCHAR* DialogKey = TEXT("ChangeVariableType_Warning");
	bool bWasSuppressed = false;
	const bool bHadKey = GConfig->GetBool(DialogSection, DialogKey, bWasSuppressed, GEditorPerProjectIni);
	GConfig->SetBool(DialogSection, DialogKey, true, GEditorPerProjectIni);

	FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, VariableName, NewType);

	if (bHadKey)
	{
		GConfig->SetBool(DialogSection, DialogKey, bWasSuppressed, GEditorPerProjectIni);
	}
	else
	{
		GConfig->RemoveKey(DialogSection, DialogKey, GEditorPerProjectIni);
	}

	const int32 NewIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
	if (NewIndex == INDEX_NONE || Blueprint->NewVariables[NewIndex].VarType != NewType)
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("%s: тип переменной «%s» не сменился."), *Blueprint->GetName(), *VariableName.ToString());
		return FString::Printf(TEXT("Error: the engine did not change the type of '%s'."), *VariableName.ToString());
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: переменная «%s» → %s."), *Blueprint->GetName(), *VariableName.ToString(), *Type);
	return FString::Printf(TEXT("OK: %s.%s is now %s. Compile and save the Blueprint; instance values of this variable were reset."),
		*Blueprint->GetName(), *VariableName.ToString(), *Type);
}
