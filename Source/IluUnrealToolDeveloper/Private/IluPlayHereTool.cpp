#include "IluPlayHereTool.h"
#include "IluUnrealToolCore.h"

#include "Components/SkinnedMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Kismet2/DebuggerCommands.h"
#include "LevelEditor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"

#define LOCTEXT_NAMESPACE "IluUnrealTool"

namespace IluPlayHere
{
	// Manual vertical offset (Ctrl+wheel), on top of the default lift setting; persists between
	// tool activations within the session.
	// Ручной вертикальный оффсет (Ctrl+колесо) поверх настройки дефолтного отступа; живёт между
	// включениями инструмента в рамках сессии.
	static float GManualLiftCm = 0.f;

	// Persistent settings (see FIluPlayHereSettings) / Сохраняемые настройки (см. FIluPlayHereSettings).
	static const TCHAR* SettingsSection = TEXT("IluUnrealTool.PlayHere");
	static float GDefaultLiftCm = 0.f;
	static bool GAlignToNormal = true;
	static bool GFaceCameraYaw = true;
	static bool GSettingsLoaded = false;

	static void LoadSettings()
	{
		if (GSettingsLoaded || !GConfig)
		{
			return;
		}
		GSettingsLoaded = true;
		GConfig->GetFloat(SettingsSection, TEXT("DefaultLiftCm"), GDefaultLiftCm, GEditorPerProjectIni);
		GConfig->GetBool(SettingsSection, TEXT("bAlignToNormal"), GAlignToNormal, GEditorPerProjectIni);
		GConfig->GetBool(SettingsSection, TEXT("bFaceCameraYaw"), GFaceCameraYaw, GEditorPerProjectIni);
		GDefaultLiftCm = FMath::Clamp(GDefaultLiftCm, -FIluPlayHereSettings::LiftMaxAbsCm, FIluPlayHereSettings::LiftMaxAbsCm);
	}
}

float FIluPlayHereSettings::GetDefaultLiftCm()
{
	IluPlayHere::LoadSettings();
	return IluPlayHere::GDefaultLiftCm;
}

void FIluPlayHereSettings::SetDefaultLiftCm(float Cm)
{
	IluPlayHere::LoadSettings();
	IluPlayHere::GDefaultLiftCm = FMath::Clamp(Cm, -LiftMaxAbsCm, LiftMaxAbsCm);
	if (GConfig)
	{
		GConfig->SetFloat(IluPlayHere::SettingsSection, TEXT("DefaultLiftCm"), IluPlayHere::GDefaultLiftCm, GEditorPerProjectIni);
	}
}

bool FIluPlayHereSettings::GetAlignToNormal()
{
	IluPlayHere::LoadSettings();
	return IluPlayHere::GAlignToNormal;
}

void FIluPlayHereSettings::SetAlignToNormal(bool bAlign)
{
	IluPlayHere::LoadSettings();
	IluPlayHere::GAlignToNormal = bAlign;
	if (GConfig)
	{
		GConfig->SetBool(IluPlayHere::SettingsSection, TEXT("bAlignToNormal"), bAlign, GEditorPerProjectIni);
	}
}

bool FIluPlayHereSettings::GetFaceCameraYaw()
{
	IluPlayHere::LoadSettings();
	return IluPlayHere::GFaceCameraYaw;
}

void FIluPlayHereSettings::SetFaceCameraYaw(bool bFace)
{
	IluPlayHere::LoadSettings();
	IluPlayHere::GFaceCameraYaw = bFace;
	if (GConfig)
	{
		GConfig->SetBool(IluPlayHere::SettingsSection, TEXT("bFaceCameraYaw"), bFace, GEditorPerProjectIni);
	}
}

float FIluPlayHereTool::GetManualLiftCm() const
{
	return IluPlayHere::GManualLiftCm;
}

void FIluPlayHereTool::SetManualLiftCm(float Cm)
{
	IluPlayHere::GManualLiftCm = Cm;
}

float FIluPlayHereTool::GetBaseYawDeg() const
{
	// При опции «по камере» базовый курс — взгляд вьюпорта под курсором.
	return FIluPlayHereSettings::GetFaceCameraYaw() ? ViewYawDeg : 0.f;
}

UClass* FIluPlayHereTool::ResolvePreviewClass(UWorld* World, FText& OutError) const
{
	UClass* GameModeClass = nullptr;
	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		GameModeClass = WorldSettings->DefaultGameMode;
	}
	if (!GameModeClass)
	{
		const FString GlobalGameMode = UGameMapsSettings::GetGlobalDefaultGameMode();
		if (!GlobalGameMode.IsEmpty())
		{
			GameModeClass = FSoftClassPath(GlobalGameMode).TryLoadClass<AGameModeBase>();
		}
	}
	if (!GameModeClass)
	{
		OutError = LOCTEXT("NoGameMode", "GameMode не найден ни в World Settings, ни в Project Settings");
		return nullptr;
	}

	const AGameModeBase* GameModeCDO = GameModeClass->GetDefaultObject<AGameModeBase>();
	UClass* PawnClass = GameModeCDO ? GameModeCDO->DefaultPawnClass.Get() : nullptr;
	if (!PawnClass)
	{
		OutError = FText::Format(
			LOCTEXT("NoPawnClass", "У GameMode «{0}» не задан DefaultPawnClass"),
			FText::FromString(GameModeClass->GetName()));
	}
	return PawnClass;
}

FText FIluPlayHereTool::GetHintText() const
{
	return LOCTEXT("PlayHereHintText",
		"ЛКМ — запустить PIE здесь • колесо — поворот (с Shift точнее) • Ctrl+колесо — высота • пробел — тогл нормали • Esc/ПКМ — отмена • «…» на тулбаре — настройки");
}

void FIluPlayHereTool::OnPreviewSpawned(AActor* Preview)
{
	// Насколько пивот выше низа павна — чтобы ставить его НА грунт, а не В грунт.
	float SkinnedBottom = 0.f;
	bool bHasSkinnedBottom = false;
	if (const USkinnedMeshComponent* Skinned = Preview->FindComponentByClass<USkinnedMeshComponent>())
	{
		// Скелетал-меш даёт самый честный низ (колёса, гусеницы, ноги).
		const FBox SkinnedBox = Skinned->Bounds.GetBox();
		if (SkinnedBox.IsValid)
		{
			SkinnedBottom = float(Preview->GetActorLocation().Z - SkinnedBox.Min.Z);
			bHasSkinnedBottom = true;
		}
	}
	const float BoundsBottom = MeasureBottomOffset(Preview);
	BottomOffset = bHasSkinnedBottom ? SkinnedBottom : BoundsBottom;
	UE_LOG(LogIluUnrealTool, Log, TEXT("PlayHere: низ павна: скелетал %.1f см, общий баунд %.1f см → выбран %.1f см"),
		SkinnedBottom, BoundsBottom, BottomOffset);
}

void FIluPlayHereTool::Confirm()
{
	const FVector Location = SpotLocation;
	const FRotator Rotation = SpotRotation.Rotator();

	RequestStop();

	TSharedPtr<IAssetViewport> DestViewport;
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		DestViewport = LevelEditor->GetFirstActiveViewport();
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("PlayHere: старт PIE в %s, рыскание %.0f°"), *Location.ToCompactString(), Rotation.Yaw);
	FPlayWorldCommandCallbacks::StartPlayFromHere(Location, Rotation, DestViewport);
}

#undef LOCTEXT_NAMESPACE
