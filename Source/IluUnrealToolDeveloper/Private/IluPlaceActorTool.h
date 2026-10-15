#pragma once

#include "CoreMinimal.h"
#include "IluPlacementToolBase.h"

/**
 * Place Actor settings, edited from the toolbar options panel ("...") and persisted in EditorPerProjectUserSettings.
 * Fully separate from the Play Here settings.
 * Настройки Place Actor, правятся из подпанели тулбара («...») и хранятся в EditorPerProjectUserSettings.
 * Полностью отдельные от настроек Play Here.
 */
struct FIluPlaceActorSettings
{
	/** Offset range, cm / Диапазон отступа, см. */
	static constexpr float LiftMaxAbsCm = FIluPlacementToolBase::LiftMaxAbsCm;

	/** Variant count range / Диапазон числа вариантов. */
	static constexpr int32 VariantCountMax = 16;

	/** Base spawn height offset, cm (Ctrl+wheel adds on top of it).
	 *  Базовый отступ по высоте при спавне, см (Ctrl+колесо добавляется поверх). */
	static float GetDefaultLiftCm();
	static void SetDefaultLiftCm(float Cm);

	/** Align the actor to the surface normal (Space also toggles while placing).
	 *  Выравнивать актор по нормали поверхности (пробел тоже тоглит в режиме расстановки). */
	static bool GetAlignToNormal();
	static void SetAlignToNormal(bool bAlign);

	/** Actor class (usually a Blueprint) picked in the "..." panel; loads it on demand.
	 *  Класс актора (обычно Blueprint) из подпанели «...»; грузится по требованию. */
	static UClass* ResolveActorClass();
	static const UClass* GetActorClassForPicker();
	static void SetActorClass(const UClass* Class);

	/** Name of the integer (int/byte/enum) variable that selects the actor variant; Z cycles it. Default "Type".
	 *  Имя целочисленной (int/byte/enum) переменной, выбирающей вариант актора; её листает Z. По умолчанию «Type». */
	static FName GetVariantProperty();
	static void SetVariantProperty(FName Name);

	/** How many variants Z cycles through: values 0..Count-1. Default 2.
	 *  Сколько вариантов листает Z: значения 0..Count-1. По умолчанию 2. */
	static int32 GetVariantCount();
	static void SetVariantCount(int32 Count);

	/** Current variant value pushed into the placed actor / Текущее значение варианта для размещаемого актора. */
	static int32 GetVariantValue();
	static void SetVariantValue(int32 Value);
};

/**
 * Interactive placement of an actor of the class picked in the "..." panel (blockout, gabarit, prop — anything):
 * a preview follows the cursor in the level viewport; LMB drops a real actor at that spot (with undo) and the mode ends.
 * Интерактивная расстановка актора класса из подпанели «...» (болванка, габарит, проп — что угодно): превью следует
 * за курсором во вьюпорте уровня; ЛКМ ставит настоящий актор (с undo) и завершает режим.
 *
 * LMB — place and exit, wheel — rotate (Shift = fine), Ctrl+wheel — lift, Z — cycle the variant,
 * Space — toggle align-to-normal, Esc/RMB — cancel.
 */
class FIluPlaceActorTool : public FIluPlacementToolBase
{
public:
	virtual const TCHAR* GetDebugName() const override { return TEXT("IluPlaceActor"); }

protected:
	virtual const TCHAR* GetLogName() const override { return TEXT("PlaceActor"); }
	virtual UClass* ResolvePreviewClass(UWorld* World, FText& OutError) const override;
	virtual FText GetHintText() const override;
	virtual void OnPreviewSpawned(AActor* Preview) override;
	virtual void Confirm() override;
	virtual bool HandleToolKeyDown(const FKey& Key) override;
	virtual bool HandleToolKeyUp(const FKey& Key) override;

	virtual bool GetAlignToNormal() const override { return FIluPlaceActorSettings::GetAlignToNormal(); }
	virtual void SetAlignToNormal(bool bAlign) override { FIluPlaceActorSettings::SetAlignToNormal(bAlign); }
	virtual float GetDefaultLiftCm() const override { return FIluPlaceActorSettings::GetDefaultLiftCm(); }
	virtual float GetManualLiftCm() const override;
	virtual void SetManualLiftCm(float Cm) override;

private:
	void CycleVariant();
};
