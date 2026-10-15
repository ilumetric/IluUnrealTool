#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class FLevelEditorViewportClient;
class UClass;
class UWorld;
struct FHitResult;
struct FKey;

/**
 * Base of the interactive "preview under the cursor" toolbar tools: a temporary preview actor follows the cursor in the
 * level viewport, LMB confirms the spot (Confirm), Esc/RMB cancels.
 * Common controls: wheel — rotate (Shift = fine), Ctrl+wheel — lift (Shift = fine), Space — toggle align-to-normal.
 * База интерактивных инструментов тулбара «превью под курсором»: временный превью-актор следует за курсором во вьюпорте
 * уровня, ЛКМ подтверждает место (Confirm), Esc/ПКМ — отмена.
 * Общее управление: колесо — поворот (Shift — точнее), Ctrl+колесо — высота (Shift — точнее), пробел — тогл нормали.
 */
class FIluPlacementToolBase : public IInputProcessor, public TSharedFromThis<FIluPlacementToolBase>
{
public:
	/** Lift offset range, cm / Диапазон отступа по высоте, см. */
	static constexpr float LiftMaxAbsCm = 500.f;

	virtual ~FIluPlacementToolBase() override = default;

	/** Spawn the preview and hook input. False — the preview class could not be resolved or spawned.
	 *  Заспавнить превью и подцепить инпут. False — класс превью не найден или не заспавнился. */
	bool Start();

	/** Tear the tool down (preview + input hooks). Safe to call from input handlers.
	 *  Свернуть инструмент (превью + инпут). Безопасно звать из инпут-хендлеров. */
	void RequestStop();

	/** The tool is still running / Инструмент ещё работает. */
	bool IsActive() const { return bStarted && !bStopRequested; }

	// IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;

protected:
	/** Short tool name for logs / Короткое имя инструмента для логов. */
	virtual const TCHAR* GetLogName() const = 0;

	/** Class of the preview actor; on failure OutError says why / Класс превью-актора; при неудаче OutError — почему. */
	virtual UClass* ResolvePreviewClass(UWorld* World, FText& OutError) const = 0;

	/** Hint shown when the tool starts / Подсказка при запуске инструмента. */
	virtual FText GetHintText() const = 0;

	/** Freshly spawned preview, collision still on: prepare it and measure BottomOffset.
	 *  Только что заспавненное превью, коллизия ещё включена: подготовить его и замерить BottomOffset. */
	virtual void OnPreviewSpawned(AActor* Preview) = 0;

	/** LMB over a valid spot (SpotLocation/SpotRotation are set) / ЛКМ по валидному месту (SpotLocation/SpotRotation заданы). */
	virtual void Confirm() = 0;

	// Per-tool settings / Настройки конкретного инструмента.
	virtual bool GetAlignToNormal() const = 0;
	virtual void SetAlignToNormal(bool bAlign) = 0;
	virtual float GetDefaultLiftCm() const = 0;
	/** Manual Ctrl+wheel offset on top of the default lift, cm / Ручной оффсет Ctrl+колесом поверх дефолтного, см. */
	virtual float GetManualLiftCm() const = 0;
	virtual void SetManualLiftCm(float Cm) = 0;

	/** Base heading the wheel rotates on top of, degrees / Базовый курс, поверх которого крутит колесо, градусы. */
	virtual float GetBaseYawDeg() const { return 0.f; }

	/** Extra keys of the tool while the cursor is over a viewport; true — handled (swallowed).
	 *  Дополнительные клавиши инструмента, пока курсор над вьюпортом; true — обработано (проглочено). */
	virtual bool HandleToolKeyDown(const FKey& Key) { return false; }
	virtual bool HandleToolKeyUp(const FKey& Key) { return false; }

	/** Distance from the actor pivot down to its bounds bottom over colliding components (fallback — all components).
	 *  Расстояние от пивота актора до низа баунда по коллизионным компонентам (фолбэк — все компоненты). */
	static float MeasureBottomOffset(AActor* Actor);

	AActor* GetPreview() const { return PreviewActor.Get(); }

	/** Current yaw set by the mouse wheel, degrees / Текущий поворот колесом мыши, градусы. */
	float YawDeg = 0.f;

	/** Yaw of the hovered viewport camera, degrees / Рыскание камеры вьюпорта под курсором, градусы. */
	float ViewYawDeg = 0.f;

	/** Distance from the actor pivot down to the bounds bottom / Расстояние от пивота актора до низа баунда. */
	float BottomOffset = 0.f;

	/** Spot picked by the preview / Место, выбранное превью. */
	FVector SpotLocation = FVector::ZeroVector;
	FQuat SpotRotation = FQuat::Identity;

private:
	bool SpawnPreview(UClass* PreviewClass);
	void DestroyPreview();
	void SetPreviewHidden(bool bHidden);
	FLevelEditorViewportClient* FindHoveredViewport() const;
	void UpdatePreviewTransform(const FHitResult& Hit);
	void OnBeginPIE(const bool bIsSimulating);
	void OnMapChange(uint32 MapChangeFlags);

	/** Preview actor in the editor world / Превью-актор в мире редактора. */
	TWeakObjectPtr<AActor> PreviewActor;

	/** Cursor was over a level viewport this frame / Курсор был над вьюпортом уровня в этом кадре. */
	bool bCursorOverViewport = false;

	/** Last trace hit is valid — LMB will confirm / Последний трейс валиден — ЛКМ подтвердит. */
	bool bHasValidSpot = false;

	bool bStarted = false;
	bool bStopRequested = false;

	FDelegateHandle BeginPIEHandle;
	FDelegateHandle MapChangeHandle;
};
