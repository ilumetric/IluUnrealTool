#include "IluPlaceActorTool.h"
#include "IluEditorNotify.h"
#include "IluUnrealToolCore.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "ScopedTransaction.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "IluUnrealTool"

namespace IluPlaceActor
{
	// Manual vertical offset (Ctrl+wheel), on top of the default lift setting; persists between
	// tool activations within the session. Separate from the Play Here one.
	// Ручной вертикальный оффсет (Ctrl+колесо) поверх настройки дефолтного отступа; живёт между
	// включениями инструмента в рамках сессии. Отдельный от Play Here.
	static float GManualLiftCm = 0.f;

	// Persistent settings (see FIluPlaceActorSettings) / Сохраняемые настройки (см. FIluPlaceActorSettings).
	static const TCHAR* SettingsSection = TEXT("IluUnrealTool.PlaceActor");
	static float GDefaultLiftCm = 0.f;
	static bool GAlignToNormal = true;
	static FString GActorClassPath;
	static FString GVariantProperty = TEXT("Type");
	static int32 GVariantCount = 2;
	static int32 GVariantValue = 0;
	static bool GSettingsLoaded = false;

	// Loaded class for the current path / Загруженный класс для текущего пути.
	static TWeakObjectPtr<UClass> GActorClassCache;

	static void LoadSettings()
	{
		if (GSettingsLoaded || !GConfig)
		{
			return;
		}
		GSettingsLoaded = true;
		GConfig->GetFloat(SettingsSection, TEXT("DefaultLiftCm"), GDefaultLiftCm, GEditorPerProjectIni);
		GConfig->GetBool(SettingsSection, TEXT("bAlignToNormal"), GAlignToNormal, GEditorPerProjectIni);
		GConfig->GetString(SettingsSection, TEXT("ActorClass"), GActorClassPath, GEditorPerProjectIni);
		GConfig->GetString(SettingsSection, TEXT("VariantProperty"), GVariantProperty, GEditorPerProjectIni);
		GConfig->GetInt(SettingsSection, TEXT("VariantCount"), GVariantCount, GEditorPerProjectIni);
		GConfig->GetInt(SettingsSection, TEXT("VariantValue"), GVariantValue, GEditorPerProjectIni);
		GDefaultLiftCm = FMath::Clamp(GDefaultLiftCm, -FIluPlaceActorSettings::LiftMaxAbsCm, FIluPlaceActorSettings::LiftMaxAbsCm);
		GVariantCount = FMath::Clamp(GVariantCount, 1, FIluPlaceActorSettings::VariantCountMax);
		GVariantValue = FMath::Clamp(GVariantValue, 0, GVariantCount - 1);
	}

	// Push the value into the actor's integer variant variable (int, byte and enum variables count).
	// Записать значение в целочисленную переменную варианта актора (подходят int, byte и enum).
	static bool ApplyVariant(AActor* Actor, int32 Value)
	{
		const FName PropertyName = FIluPlaceActorSettings::GetVariantProperty();
		if (!Actor || PropertyName.IsNone())
		{
			return false;
		}
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(PropertyName);
		if (!Prop)
		{
			return false;
		}
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
		if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			IntProp->SetPropertyValue(ValuePtr, Value);
			return true;
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			ByteProp->SetPropertyValue(ValuePtr, uint8(Value));
			return true;
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, int64(Value));
			return true;
		}
		return false;
	}
}

float FIluPlaceActorSettings::GetDefaultLiftCm()
{
	IluPlaceActor::LoadSettings();
	return IluPlaceActor::GDefaultLiftCm;
}

void FIluPlaceActorSettings::SetDefaultLiftCm(float Cm)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GDefaultLiftCm = FMath::Clamp(Cm, -LiftMaxAbsCm, LiftMaxAbsCm);
	if (GConfig)
	{
		GConfig->SetFloat(IluPlaceActor::SettingsSection, TEXT("DefaultLiftCm"), IluPlaceActor::GDefaultLiftCm, GEditorPerProjectIni);
	}
}

bool FIluPlaceActorSettings::GetAlignToNormal()
{
	IluPlaceActor::LoadSettings();
	return IluPlaceActor::GAlignToNormal;
}

void FIluPlaceActorSettings::SetAlignToNormal(bool bAlign)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GAlignToNormal = bAlign;
	if (GConfig)
	{
		GConfig->SetBool(IluPlaceActor::SettingsSection, TEXT("bAlignToNormal"), bAlign, GEditorPerProjectIni);
	}
}

UClass* FIluPlaceActorSettings::ResolveActorClass()
{
	IluPlaceActor::LoadSettings();
	if (UClass* Cached = IluPlaceActor::GActorClassCache.Get())
	{
		return Cached;
	}
	if (IluPlaceActor::GActorClassPath.IsEmpty())
	{
		return nullptr;
	}
	UClass* Loaded = FSoftClassPath(IluPlaceActor::GActorClassPath).TryLoadClass<AActor>();
	IluPlaceActor::GActorClassCache = Loaded;
	if (!Loaded)
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("PlaceActor: класс «%s» не загрузился"), *IluPlaceActor::GActorClassPath);
	}
	return Loaded;
}

const UClass* FIluPlaceActorSettings::GetActorClassForPicker()
{
	return ResolveActorClass();
}

void FIluPlaceActorSettings::SetActorClass(const UClass* Class)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GActorClassCache = const_cast<UClass*>(Class);
	IluPlaceActor::GActorClassPath = Class ? FSoftClassPath(Class).ToString() : FString();
	if (GConfig)
	{
		GConfig->SetString(IluPlaceActor::SettingsSection, TEXT("ActorClass"), *IluPlaceActor::GActorClassPath, GEditorPerProjectIni);
	}
	UE_LOG(LogIluUnrealTool, Log, TEXT("PlaceActor: выбран класс «%s»"), Class ? *Class->GetName() : TEXT("нет"));
}

FName FIluPlaceActorSettings::GetVariantProperty()
{
	IluPlaceActor::LoadSettings();
	return FName(*IluPlaceActor::GVariantProperty);
}

void FIluPlaceActorSettings::SetVariantProperty(FName Name)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GVariantProperty = Name.IsNone() ? FString() : Name.ToString();
	if (GConfig)
	{
		GConfig->SetString(IluPlaceActor::SettingsSection, TEXT("VariantProperty"), *IluPlaceActor::GVariantProperty, GEditorPerProjectIni);
	}
}

int32 FIluPlaceActorSettings::GetVariantCount()
{
	IluPlaceActor::LoadSettings();
	return IluPlaceActor::GVariantCount;
}

void FIluPlaceActorSettings::SetVariantCount(int32 Count)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GVariantCount = FMath::Clamp(Count, 1, VariantCountMax);
	if (GConfig)
	{
		GConfig->SetInt(IluPlaceActor::SettingsSection, TEXT("VariantCount"), IluPlaceActor::GVariantCount, GEditorPerProjectIni);
	}
	// Текущее значение не должно вылезать за новый диапазон.
	if (IluPlaceActor::GVariantValue >= IluPlaceActor::GVariantCount)
	{
		SetVariantValue(IluPlaceActor::GVariantCount - 1);
	}
}

int32 FIluPlaceActorSettings::GetVariantValue()
{
	IluPlaceActor::LoadSettings();
	return IluPlaceActor::GVariantValue;
}

void FIluPlaceActorSettings::SetVariantValue(int32 Value)
{
	IluPlaceActor::LoadSettings();
	IluPlaceActor::GVariantValue = FMath::Clamp(Value, 0, IluPlaceActor::GVariantCount - 1);
	if (GConfig)
	{
		GConfig->SetInt(IluPlaceActor::SettingsSection, TEXT("VariantValue"), IluPlaceActor::GVariantValue, GEditorPerProjectIni);
	}
}

float FIluPlaceActorTool::GetManualLiftCm() const
{
	return IluPlaceActor::GManualLiftCm;
}

void FIluPlaceActorTool::SetManualLiftCm(float Cm)
{
	IluPlaceActor::GManualLiftCm = Cm;
}

UClass* FIluPlaceActorTool::ResolvePreviewClass(UWorld* World, FText& OutError) const
{
	UClass* ActorClass = FIluPlaceActorSettings::ResolveActorClass();
	if (!ActorClass)
	{
		OutError = LOCTEXT("PlaceActorNoClass",
			"Класс актора для Place Actor не задан или не загрузился — выберите его в подпанели «…» на тулбаре");
	}
	return ActorClass;
}

FText FIluPlaceActorTool::GetHintText() const
{
	return FText::Format(LOCTEXT("PlaceActorHintText",
		"ЛКМ — поставить актор и выйти • колесо — поворот (с Shift точнее) • Ctrl+колесо — высота • Z — листать {0} • пробел — тогл нормали • Esc/ПКМ — отмена • «…» на тулбаре — настройки"),
		FText::FromName(FIluPlaceActorSettings::GetVariantProperty()));
}

void FIluPlaceActorTool::OnPreviewSpawned(AActor* Preview)
{
	// Сначала применяем текущий вариант (конструкшн-скрипт может менять геометрию), потом меряем низ.
	if (IluPlaceActor::ApplyVariant(Preview, FIluPlaceActorSettings::GetVariantValue()))
	{
		Preview->RerunConstructionScripts();
	}
	else
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("PlaceActor: у класса %s нет целочисленной переменной «%s» — листание по Z работать не будет"),
			*Preview->GetClass()->GetName(), *FIluPlaceActorSettings::GetVariantProperty().ToString());
	}
	BottomOffset = MeasureBottomOffset(Preview);
	UE_LOG(LogIluUnrealTool, Log, TEXT("PlaceActor: %s = %d"),
		*FIluPlaceActorSettings::GetVariantProperty().ToString(), FIluPlaceActorSettings::GetVariantValue());
}

bool FIluPlaceActorTool::HandleToolKeyDown(const FKey& Key)
{
	// Нажатие Z только глотаем (иначе сработает шорткат редактора) — листаем на отжатии,
	// чтобы удержание клавиши не гнало автоповтор.
	return Key == EKeys::Z;
}

bool FIluPlaceActorTool::HandleToolKeyUp(const FKey& Key)
{
	if (Key == EKeys::Z)
	{
		CycleVariant();
		return true;
	}
	return false;
}

void FIluPlaceActorTool::CycleVariant()
{
	// Листаем по кругу: 0 → 1 → … → Count-1 → 0.
	const int32 Value = (FIluPlaceActorSettings::GetVariantValue() + 1) % FIluPlaceActorSettings::GetVariantCount();
	FIluPlaceActorSettings::SetVariantValue(Value);

	AActor* Preview = GetPreview();
	if (Preview && IluPlaceActor::ApplyVariant(Preview, Value))
	{
		// Геометрия могла смениться — перегоняем конструкшн-скрипт и меряем низ заново.
		Preview->RerunConstructionScripts();
		BottomOffset = MeasureBottomOffset(Preview);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}
	UE_LOG(LogIluUnrealTool, Log, TEXT("PlaceActor: %s = %d"), *FIluPlaceActorSettings::GetVariantProperty().ToString(), Value);
}

void FIluPlaceActorTool::Confirm()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* ActorClass = FIluPlaceActorSettings::ResolveActorClass();
	if (!World || !World->GetCurrentLevel() || !ActorClass)
	{
		return;
	}

	// Настоящий актор с undo; после установки режим завершается (см. конец функции).
	const FScopedTransaction Transaction(LOCTEXT("PlaceActorTransaction", "Place Actor"));
	AActor* Placed = GEditor->AddActor(World->GetCurrentLevel(), ActorClass,
		FTransform(SpotRotation, SpotLocation), /*bSilent*/ true, RF_Transactional, /*bSelectActor*/ false);
	if (!Placed)
	{
		FIluEditorNotify::Show(LOCTEXT("PlaceActorFailed", "Не удалось поставить актор"), false);
		UE_LOG(LogIluUnrealTool, Warning, TEXT("PlaceActor: не удалось поставить %s"), *ActorClass->GetName());
		return;
	}

	Placed->Modify();
	const int32 VariantValue = FIluPlaceActorSettings::GetVariantValue();
	if (IluPlaceActor::ApplyVariant(Placed, VariantValue))
	{
		Placed->RerunConstructionScripts();
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("PlaceActor: поставлен %s в %s, рыскание %.0f°, %s %d"),
		*Placed->GetName(), *SpotLocation.ToCompactString(), SpotRotation.Rotator().Yaw,
		*FIluPlaceActorSettings::GetVariantProperty().ToString(), VariantValue);

	// Поставили — операция завершена.
	RequestStop();
}

#undef LOCTEXT_NAMESPACE
