#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class SWidget;
class UClass;
class UWorld;

/**
 * Time Of Day wheel settings, edited from the toolbar options panel ("...") and persisted
 * in EditorPerProjectUserSettings.
 * Настройки колеса Time Of Day, правятся из подпанели тулбара («...») и хранятся
 * в EditorPerProjectUserSettings.
 */
struct FIluTimeOfDaySettings
{
	/** Sky system actor class (usually the Ultra Dynamic Sky Blueprint). None — auto-detect
	 *  an actor whose class name starts with "Ultra_Dynamic_Sky".
	 *  Класс актора системы неба (обычно Blueprint Ultra Dynamic Sky). None — автопоиск
	 *  актора, чей класс начинается с «Ultra_Dynamic_Sky». */
	static UClass* ResolveSkyClass();
	static const UClass* GetSkyClassForPicker();
	static void SetSkyClass(const UClass* Class);
};

/**
 * Viewport-toolbar Time Of Day wheel: finds the sky actor (Ultra Dynamic Sky) in the current
 * world and drives its "Time of Day" variable via reflection. Endless: the day wraps around,
 * there are no end stops.
 * Колесо Time Of Day на тулбаре вьюпорта: находит актор неба (Ultra Dynamic Sky) в текущем
 * мире и крутит его переменную «Time of Day» через рефлексию. Бесконечное: сутки
 * заворачиваются, упоров нет.
 */
struct FIluTimeOfDayTool
{
	/** UDS time range: 0..2400, fractional hours x100 (960 = 9:36) / Диапазон времени UDS. */
	static constexpr float TimeMin = 0.f;
	static constexpr float TimeMax = 2400.f;

	/** Build the wheel widget for the viewport toolbar / Собрать виджет колеса для тулбара вьюпорта. */
	static TSharedRef<SWidget> MakeToolbarWidget();

	/** Sky actor in the active world (PIE if playing, else editor), nullptr if none.
	 *  Актор неба в активном мире (PIE, если играем, иначе редактор), nullptr если нет. */
	static AActor* ResolveSkyActor();

	/** Current Time of Day read from the sky actor; falls back to the last known value.
	 *  Текущий Time of Day из актора неба; фолбэк — последнее известное значение. */
	static float GetTimeOfDay();

	/** Push Time of Day into the sky actor, wrapping it into 0..2400 (editor: + rerun construction, dirty, redraw).
	 *  Записать Time of Day в актор неба с заворотом в 0..2400 (в редакторе: + construction script, dirty, redraw). */
	static void SetTimeOfDay(float Time);
};
