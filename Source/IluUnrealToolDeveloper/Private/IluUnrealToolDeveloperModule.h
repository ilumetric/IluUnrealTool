#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FIluPlaceActorTool;
class FIluPlayHereTool;
class SWidget;

/**
 * Developer section: toolbar buttons next to the Play block and the Time Of Day wheel on the viewport toolbar.
 * Раздел разработчика: кнопки тулбара рядом с блоком Play и колесо Time Of Day на тулбаре вьюпорта.
 */
class FIluUnrealToolDeveloperModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void TogglePlayHereTool();
	bool IsPlayHereToolActive() const;
	void TogglePlaceActorTool();
	bool IsPlaceActorToolActive() const;

	/** Options subpanel for the "..." combo button / Подпанель настроек для кнопки «...». */
	TSharedRef<SWidget> MakeToolOptionsMenu();

	TSharedPtr<FIluPlayHereTool> PlayHereTool;
	TSharedPtr<FIluPlaceActorTool> PlaceActorTool;
};
