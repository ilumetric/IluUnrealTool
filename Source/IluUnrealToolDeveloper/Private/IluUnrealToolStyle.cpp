#include "IluUnrealToolStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

namespace IluUnrealToolStyle
{
	static TSharedPtr<FSlateStyleSet> GStyleSet;
}

FName FIluUnrealToolStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("IluUnrealToolStyle"));
	return StyleSetName;
}

void FIluUnrealToolStyle::Initialize()
{
	using namespace IluUnrealToolStyle;
	if (GStyleSet.IsValid())
	{
		return;
	}

	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("IluUnrealTool")))
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	// Тот же размер 20x20, что у штатных иконок блока Play (StarshipStyle: PlayWorld.PlayInViewport).
#define RootToContentDir Style->RootToContentDir
	Style->Set("IluUnrealTool.PlayHere", new IMAGE_BRUSH_SVG("Icons/PlayHere", CoreStyleConstants::Icon20x20));
	Style->Set("IluUnrealTool.PlaceActor", new IMAGE_BRUSH_SVG("Icons/PlaceActor", CoreStyleConstants::Icon20x20));
#undef RootToContentDir

	FSlateStyleRegistry::RegisterSlateStyle(*Style);
	GStyleSet = Style;
}

void FIluUnrealToolStyle::Shutdown()
{
	using namespace IluUnrealToolStyle;
	if (GStyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*GStyleSet);
		GStyleSet.Reset();
	}
}
