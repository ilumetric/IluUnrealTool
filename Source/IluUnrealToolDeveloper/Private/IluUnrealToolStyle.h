#pragma once

#include "CoreMinimal.h"

class ISlateStyle;

/**
 * Slate style of the plugin: toolbar icons from Resources/Icons (SVG).
 * Slate-стиль плагина: иконки тулбара из Resources/Icons (SVG).
 */
struct FIluUnrealToolStyle
{
	static void Initialize();
	static void Shutdown();

	/** Style set name for FSlateIcon / Имя стиля для FSlateIcon. */
	static FName GetStyleSetName();
};
