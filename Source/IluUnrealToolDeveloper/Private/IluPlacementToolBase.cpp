#include "IluPlacementToolBase.h"
#include "IluEditorNotify.h"
#include "IluUnrealToolCore.h"

#include "CollisionQueryParams.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "LevelEditorViewport.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "IluUnrealTool"

namespace IluPlacement
{
	// Cursor ray length, cm / Длина луча под курсором, см.
	constexpr double TraceLengthCm = 1000000.0;
	// Extra clearance between the bounds bottom and the ground, cm / Зазор между низом баунда и грунтом, см.
	constexpr double GroundClearanceCm = 2.0;
	// Yaw step per mouse wheel notch, degrees / Шаг рыскания на щелчок колеса, градусы.
	constexpr float YawStepDeg = 15.f;
	constexpr float YawStepFineDeg = 5.f;
	// Manual lift step per wheel notch with Ctrl held, cm / Шаг ручного подъёма колесом с зажатым Ctrl, см.
	constexpr float LiftStepCm = 10.f;
	constexpr float LiftStepFineCm = 2.f;
	// Normals with Z below this are walls, not ground / Нормали с Z ниже порога — стены, не опора.
	constexpr double MinGroundNormalZ = 0.2;
}

bool FIluPlacementToolBase::Start()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return false;
	}

	FText Error;
	UClass* PreviewClass = ResolvePreviewClass(World, Error);
	if (!PreviewClass)
	{
		FIluEditorNotify::Show(Error, false);
		UE_LOG(LogIluUnrealTool, Warning, TEXT("%s: класс превью не найден: %s"), GetLogName(), *Error.ToString());
		return false;
	}

	if (!SpawnPreview(PreviewClass))
	{
		FIluEditorNotify::Show(FText::Format(LOCTEXT("PreviewSpawnFailed", "Не удалось создать превью «{0}»"),
			FText::FromString(PreviewClass->GetName())), false);
		UE_LOG(LogIluUnrealTool, Warning, TEXT("%s: не удалось заспавнить превью %s"), GetLogName(), *PreviewClass->GetName());
		return false;
	}

	FSlateApplication::Get().RegisterInputPreProcessor(SharedThis(this));
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddSP(this, &FIluPlacementToolBase::OnBeginPIE);
	MapChangeHandle = FEditorDelegates::MapChange.AddSP(this, &FIluPlacementToolBase::OnMapChange);
	bStarted = true;

	FIluEditorNotify::Show(GetHintText(), true);
	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: режим расстановки включён (%s)"), GetLogName(), *PreviewClass->GetName());
	return true;
}

void FIluPlacementToolBase::RequestStop()
{
	if (bStopRequested)
	{
		return;
	}
	bStopRequested = true;

	DestroyPreview();
	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	FEditorDelegates::MapChange.Remove(MapChangeHandle);

	if (!bStarted)
	{
		return;
	}

	// Снятие препроцессора откладываем на следующий тик: сейчас Slate может итерироваться
	// по списку процессоров (мы бываем внутри собственного хендлера).
	TSharedRef<FIluPlacementToolBase> Self = AsShared();
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([Self]()
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(Self);
			}
		}));
	}
	else if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Self);
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: режим расстановки выключен"), GetLogName());
}

void FIluPlacementToolBase::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (bStopRequested)
	{
		return;
	}

	AActor* Preview = PreviewActor.Get();
	if (!Preview)
	{
		// Мир сменился или превью удалили извне — сворачиваемся.
		RequestStop();
		return;
	}

	FLevelEditorViewportClient* Client = FindHoveredViewport();
	bCursorOverViewport = (Client != nullptr);
	bHasValidSpot = false;

	if (!Client)
	{
		SetPreviewHidden(true);
		return;
	}

	UWorld* World = Client->GetWorld();
	if (!World)
	{
		SetPreviewHidden(true);
		return;
	}

	// Направление взгляда именно того вьюпорта, над которым курсор: инструмент может брать его
	// базовым курсом (GetBaseYawDeg; в ортогональных видах это верх экрана).
	ViewYawDeg = float(Client->GetViewRotation().Yaw);

	const FViewportCursorLocation CursorLoc = Client->GetCursorWorldLocationFromMousePos();
	const FVector TraceStart = CursorLoc.GetOrigin();
	const FVector TraceEnd = TraceStart + CursorLoc.GetDirection() * IluPlacement::TraceLengthCm;

	FCollisionQueryParams QueryParams(FName(GetDebugName()), /*bTraceComplex*/ true, Preview);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		UpdatePreviewTransform(Hit);
		SetPreviewHidden(false);
		bHasValidSpot = true;
		Client->Invalidate(false, false);
	}
	else
	{
		SetPreviewHidden(true);
	}
}

bool FIluPlacementToolBase::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (!IsActive())
	{
		return false;
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		RequestStop();
		return true;
	}
	if (!bCursorOverViewport)
	{
		return false;
	}
	if (Key == EKeys::SpaceBar)
	{
		const bool bAlign = !GetAlignToNormal();
		SetAlignToNormal(bAlign);
		UE_LOG(LogIluUnrealTool, Log, TEXT("%s: выравнивание по нормали %s"), GetLogName(), bAlign ? TEXT("ВКЛ") : TEXT("ВЫКЛ"));
		return true;
	}
	return HandleToolKeyDown(Key);
}

bool FIluPlacementToolBase::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (!IsActive() || !bCursorOverViewport)
	{
		return false;
	}
	return HandleToolKeyUp(InKeyEvent.GetKey());
}

bool FIluPlacementToolBase::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!IsActive() || !bCursorOverViewport)
	{
		return false;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bHasValidSpot)
		{
			Confirm();
		}
		// Глотаем клик в любом случае, чтобы редактор не начал выделение под инструментом.
		return true;
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		RequestStop();
		return true;
	}
	return false;
}

bool FIluPlacementToolBase::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Отпускания ЛКМ/ПКМ над вьюпортом тоже глотаем, чтобы виджет не получил клик без пары.
	if (!IsActive() || !bCursorOverViewport)
	{
		return false;
	}
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		|| MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
}

bool FIluPlacementToolBase::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	if (!IsActive() || !bCursorOverViewport)
	{
		return false;
	}

	if (InWheelEvent.IsControlDown())
	{
		// Ctrl+колесо — ручной вертикальный оффсет превью (и точки спавна) поверх дефолтного.
		const float Step = InWheelEvent.IsShiftDown() ? IluPlacement::LiftStepFineCm : IluPlacement::LiftStepCm;
		const float ManualLiftCm = FMath::Clamp(GetManualLiftCm() + InWheelEvent.GetWheelDelta() * Step, -LiftMaxAbsCm, LiftMaxAbsCm);
		SetManualLiftCm(ManualLiftCm);
		UE_LOG(LogIluUnrealTool, Log, TEXT("%s: ручной оффсет высоты %.0f см (итог с дефолтным %.0f см)"),
			GetLogName(), ManualLiftCm, ManualLiftCm + GetDefaultLiftCm());
		return true;
	}

	const float Step = InWheelEvent.IsShiftDown() ? IluPlacement::YawStepFineDeg : IluPlacement::YawStepDeg;
	YawDeg = FMath::UnwindDegrees(YawDeg + InWheelEvent.GetWheelDelta() * Step);
	return true;
}

float FIluPlacementToolBase::MeasureBottomOffset(AActor* Actor)
{
	if (!Actor)
	{
		return 0.f;
	}

	// Баунд по коллизионным компонентам честнее (без спрайтов/иконок/камеры). Если коллизия актора
	// выключена — на время замера включаем обратно, иначе сработает раздутый фолбэк по всем компонентам.
	const bool bHadCollision = Actor->GetActorEnableCollision();
	if (!bHadCollision)
	{
		Actor->SetActorEnableCollision(true);
	}
	FBox LocalBounds = Actor->CalculateComponentsBoundingBoxInLocalSpace(/*bNonColliding*/ false, /*bIncludeFromChildActors*/ true);
	if (!bHadCollision)
	{
		Actor->SetActorEnableCollision(false);
	}
	if (!LocalBounds.IsValid)
	{
		LocalBounds = Actor->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	}
	return LocalBounds.IsValid ? float(-LocalBounds.Min.Z) : 0.f;
}

bool FIluPlacementToolBase::SpawnPreview(UClass* PreviewClass)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transient | RF_DuplicateTransient;
	Params.bTemporaryEditorActor = true;
	Params.bHideFromSceneOutliner = true;
	Params.bNoFail = true;

	// Спавним глубоко под уровнем и прячем — на экран превью попадёт после первого трейса.
	AActor* Preview = World->SpawnActor<AActor>(PreviewClass, FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, -1000000.0)), Params);
	if (!Preview)
	{
		return false;
	}
	PreviewActor = Preview;

	// ВАЖНО: подготовка и замер низа — ДО SetActorEnableCollision(false), см. MeasureBottomOffset.
	OnPreviewSpawned(Preview);

	Preview->SetActorEnableCollision(false);
	Preview->SetIsTemporarilyHiddenInEditor(true);
	return true;
}

void FIluPlacementToolBase::DestroyPreview()
{
	if (AActor* Preview = PreviewActor.Get())
	{
		if (UWorld* World = Preview->GetWorld())
		{
			World->DestroyActor(Preview);
		}
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}
	PreviewActor.Reset();
}

void FIluPlacementToolBase::SetPreviewHidden(bool bHidden)
{
	AActor* Preview = PreviewActor.Get();
	if (!Preview || Preview->IsTemporarilyHiddenInEditor() == bHidden)
	{
		return;
	}
	Preview->SetIsTemporarilyHiddenInEditor(bHidden);
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

FLevelEditorViewportClient* FIluPlacementToolBase::FindHoveredViewport() const
{
	if (!GEditor)
	{
		return nullptr;
	}
	const AActor* Preview = PreviewActor.Get();
	const UWorld* PreviewWorld = Preview ? Preview->GetWorld() : nullptr;

	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (!Client || !Client->Viewport || !Client->IsVisible() || Client->GetWorld() != PreviewWorld)
		{
			continue;
		}
		const FIntPoint Size = Client->Viewport->GetSizeXY();
		if (Size.X <= 0 || Size.Y <= 0)
		{
			continue;
		}
		const int32 MouseX = Client->Viewport->GetMouseX();
		const int32 MouseY = Client->Viewport->GetMouseY();
		if (MouseX >= 0 && MouseY >= 0 && MouseX < Size.X && MouseY < Size.Y)
		{
			return Client;
		}
	}
	return nullptr;
}

void FIluPlacementToolBase::UpdatePreviewTransform(const FHitResult& Hit)
{
	AActor* Preview = PreviewActor.Get();
	if (!Preview)
	{
		return;
	}

	FVector Up = FVector::UpVector;
	if (GetAlignToNormal())
	{
		const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		// Отвесные поверхности опорой не считаем — актор не заваливаем набок.
		if (Normal.Z > IluPlacement::MinGroundNormalZ)
		{
			Up = Normal;
		}
	}

	// Колесо крутит поверх базового курса, а не вместо него.
	const FQuat AlignQuat = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	const FQuat Rotation = AlignQuat * FQuat(FVector::UpVector, FMath::DegreesToRadians(GetBaseYawDeg() + YawDeg));
	const FVector Location = Hit.ImpactPoint + Up * (BottomOffset + IluPlacement::GroundClearanceCm
		+ GetDefaultLiftCm() + GetManualLiftCm());

	Preview->SetActorLocationAndRotation(Location, Rotation);
	SpotLocation = Location;
	SpotRotation = Rotation;
}

void FIluPlacementToolBase::OnBeginPIE(const bool bIsSimulating)
{
	RequestStop();
}

void FIluPlacementToolBase::OnMapChange(uint32 MapChangeFlags)
{
	RequestStop();
}

#undef LOCTEXT_NAMESPACE
