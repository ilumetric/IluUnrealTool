#pragma once

#include "CoreMinimal.h"
#include "IluPlacementToolBase.h"

/**
 * Play Here settings, edited from the toolbar options panel ("...") and persisted in EditorPerProjectUserSettings.
 * Настройки Play Here, правятся из подпанели тулбара («...») и хранятся в EditorPerProjectUserSettings.
 */
struct FIluPlayHereSettings
{
	/** Offset range, cm / Диапазон отступа, см. */
	static constexpr float LiftMaxAbsCm = FIluPlacementToolBase::LiftMaxAbsCm;

	/** Base spawn height offset, cm (Ctrl+wheel adds on top of it).
	 *  Базовый отступ по высоте при спавне, см (Ctrl+колесо добавляется поверх). */
	static float GetDefaultLiftCm();
	static void SetDefaultLiftCm(float Cm);

	/** Align the pawn to the surface normal (Space also toggles while placing).
	 *  Выравнивать павна по нормали поверхности (пробел тоже тоглит в режиме расстановки). */
	static bool GetAlignToNormal();
	static void SetAlignToNormal(bool bAlign);

	/** Take the viewport camera yaw as the base heading (the wheel then rotates on top of it).
	 *  Брать рыскание камеры вьюпорта за базовое направление (колесо крутит поверх него). */
	static bool GetFaceCameraYaw();
	static void SetFaceCameraYaw(bool bFace);
};

/**
 * Interactive "play from here": a preview of the GameMode default pawn (vehicle, character, anything) follows the cursor
 * in the level viewport; LMB starts PIE at that spot with that heading.
 * Интерактивный «запуск отсюда»: превью дефолтного павна из GameMode (машина, персонаж — что угодно) следует за курсором
 * во вьюпорте уровня; ЛКМ запускает PIE с этого места и с этим курсом.
 *
 * LMB — start PIE, wheel — rotate (Shift = fine), Ctrl+wheel — lift, Space — toggle align-to-normal, Esc/RMB — cancel.
 */
class FIluPlayHereTool : public FIluPlacementToolBase
{
public:
	virtual const TCHAR* GetDebugName() const override { return TEXT("IluPlayHere"); }

protected:
	virtual const TCHAR* GetLogName() const override { return TEXT("PlayHere"); }
	virtual UClass* ResolvePreviewClass(UWorld* World, FText& OutError) const override;
	virtual FText GetHintText() const override;
	virtual void OnPreviewSpawned(AActor* Preview) override;
	virtual void Confirm() override;

	virtual bool GetAlignToNormal() const override { return FIluPlayHereSettings::GetAlignToNormal(); }
	virtual void SetAlignToNormal(bool bAlign) override { FIluPlayHereSettings::SetAlignToNormal(bAlign); }
	virtual float GetDefaultLiftCm() const override { return FIluPlayHereSettings::GetDefaultLiftCm(); }
	virtual float GetManualLiftCm() const override;
	virtual void SetManualLiftCm(float Cm) override;
	virtual float GetBaseYawDeg() const override;
};
