#include "IluUnrealToolDeveloperModule.h"
#include "IluMeshOpsMenu.h"
#include "IluPlaceActorTool.h"
#include "IluPlayHereTool.h"
#include "IluTimeOfDayTool.h"
#include "IluUnrealToolStyle.h"

#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "IluUnrealTool"

void FIluUnrealToolDeveloperModule::StartupModule()
{
	FIluUnrealToolStyle::Initialize();
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FIluUnrealToolDeveloperModule::RegisterMenus));
}

void FIluUnrealToolDeveloperModule::ShutdownModule()
{
	if (PlayHereTool.IsValid())
	{
		PlayHereTool->RequestStop();
		PlayHereTool.Reset();
	}
	if (PlaceActorTool.IsValid())
	{
		PlaceActorTool->RequestStop();
		PlaceActorTool.Reset();
	}
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FIluUnrealToolStyle::Shutdown();
}

void FIluUnrealToolDeveloperModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Секция сразу после блока Play на тулбаре уровня.
	UToolMenu* PlayToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = PlayToolBar->FindOrAddSection("IluUnrealTool");
	Section.AddSeparator("IluUnrealToolSeparator");

	// Стили Toolbar.Backplate* — тот же вид (пилюля, размер, отступы), что у штатного блока
	// Play (см. FPlayWorldCommands::BuildToolbar в DebuggerCommands.cpp). Backplate рисуется
	// только у обычных кнопок (Button): ToggleButton рендерится чекбоксом со своим фоном,
	// поэтому подсветки «включено» у кнопок нет — режим виден по превью под курсором.
	// Пилюля из трёх сегментов: Play Here (Left) → Place Actor (Center) → «⋮» (RightCombo).
	FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
		"IluPlayHere",
		FUIAction(
			FExecuteAction::CreateRaw(this, &FIluUnrealToolDeveloperModule::TogglePlayHereTool),
			FCanExecuteAction::CreateLambda([]() { return GEditor && GEditor->PlayWorld == nullptr; })
		),
		LOCTEXT("PlayHereLabel", "Play Here"),
		LOCTEXT("PlayHereTooltip",
			"Place the GameMode default pawn under the cursor and start PIE at that spot. LMB — play, mouse wheel — rotate (Shift = fine), Ctrl+wheel — lift, Space — toggle align-to-normal, Esc/RMB — cancel.\n"
			"Поставить дефолтного павна из GameMode под курсором и запустить PIE с этого места. ЛКМ — старт, колесо — поворот (с Shift точнее), Ctrl+колесо — высота, пробел — тогл выравнивания по нормали, Esc/ПКМ — отмена."),
		FSlateIcon(FIluUnrealToolStyle::GetStyleSetName(), "IluUnrealTool.PlayHere"),
		EUserInterfaceActionType::Button
	);
	Entry.StyleNameOverride = FName("Toolbar.BackplateLeft");
	Section.AddEntry(Entry);

	// Кнопка «Place Actor» — расстановка актора (класс выбирается в «⋮»).
	FToolMenuEntry PlaceActorEntry = FToolMenuEntry::InitToolBarButton(
		"IluPlaceActor",
		FUIAction(
			FExecuteAction::CreateRaw(this, &FIluUnrealToolDeveloperModule::TogglePlaceActorTool),
			FCanExecuteAction::CreateLambda([]() { return GEditor && GEditor->PlayWorld == nullptr; })
		),
		LOCTEXT("PlaceActorLabel", "Place Actor"),
		LOCTEXT("PlaceActorTooltip",
			"Place an actor of the class picked in the \"...\" panel under the cursor. LMB — place and exit, wheel — rotate, Ctrl+wheel — lift, Z — cycle the variant variable, Space — align-to-normal, Esc/RMB — cancel.\n"
			"Поставить актор класса из подпанели «…» под курсором. ЛКМ — поставить и выйти, колесо — поворот, Ctrl+колесо — высота, Z — листать переменную варианта, пробел — нормаль, Esc/ПКМ — отмена."),
		FSlateIcon(FIluUnrealToolStyle::GetStyleSetName(), "IluUnrealTool.PlaceActor"),
		EUserInterfaceActionType::Button
	);
	PlaceActorEntry.StyleNameOverride = FName("Toolbar.BackplateCenter");
	Section.AddEntry(PlaceActorEntry);

	// Кнопка «...» — подпанель настроек инструментов. Иконку не задаём:
	// три точки — это DownArrowImage стиля BackplateRightCombo (как у штатного PIECombo).
	FToolMenuEntry OptionsEntry = FToolMenuEntry::InitComboButton(
		"IluUnrealToolOptions",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FIluUnrealToolDeveloperModule::MakeToolOptionsMenu),
		LOCTEXT("IluOptionsLabel", "IluUnrealTool Options"),
		LOCTEXT("IluOptionsTooltip",
			"Play Here and Place Actor settings, Time Of Day sky class, Mesh Ops for the selected actors.\n"
			"Настройки Play Here и Place Actor, класс неба для Time Of Day, Mesh Ops для выделенных акторов.")
	);
	OptionsEntry.StyleNameOverride = FName("Toolbar.BackplateRightCombo");
	Section.AddEntry(OptionsEntry);

	// Слайдер Time Of Day — на тулбаре вьюпорта (5.8: LevelEditor.ViewportToolbar), сразу
	// после левой секции (Transform/Snapping); правая секция прижата вправо (Alignment=Last),
	// так что виджет встаёт в пустую зону между ними — как в UEFN.
	UToolMenu* ViewportToolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.ViewportToolbar");
	FToolMenuSection& TodSection = ViewportToolbar->FindOrAddSection("IluUnrealToolTimeOfDay");
	// Тулбар генерится на каждый вьюпорт отдельно — виджет обязан строиться заново
	// (MakeCustomWidget), один общий инстанс через InitWidget Slate переродичить не даст.
	FToolMenuEntry TodEntry = FToolMenuEntry::InitWidget(
		"IluTimeOfDaySlider", SNullWidget::NullWidget, FText::GetEmpty(), /*bNoIndent*/ true);
	TodEntry.MakeCustomWidget.BindLambda(
		[](const FToolMenuContext&, const FToolMenuCustomWidgetContext&)
		{
			return FIluTimeOfDayTool::MakeToolbarWidget();
		});
	TodSection.AddEntry(TodEntry);
}

TSharedRef<SWidget> FIluUnrealToolDeveloperModule::MakeToolOptionsMenu()
{
	// Меню не закрываем после клика — значения удобно крутить, глядя на превью.
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ false, /*InCommandList*/ nullptr);

	MenuBuilder.BeginSection("IluPlayHereOptions", LOCTEXT("PlayHereOptionsSection", "Play Here"));

	// Базовый отступ по высоте: применяется всегда; Ctrl+колесо в режиме расстановки — поверх него.
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(100.f)
		[
			SNew(SSpinBox<float>)
			.MinValue(-FIluPlayHereSettings::LiftMaxAbsCm)
			.MaxValue(FIluPlayHereSettings::LiftMaxAbsCm)
			.Delta(1.f)
			.ToolTipText(LOCTEXT("DefaultLiftTooltip",
				"Base spawn height offset in cm, always applied; Ctrl+wheel while placing adds on top.\n"
				"Базовый отступ по высоте в см, применяется всегда; Ctrl+колесо в режиме расстановки — поверх него."))
			.Value_Static(&FIluPlayHereSettings::GetDefaultLiftCm)
			.OnValueChanged_Static(&FIluPlayHereSettings::SetDefaultLiftCm)
		],
		LOCTEXT("DefaultLiftLabel", "Default lift (cm)"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AlignToNormalLabel", "Align to surface normal"),
		LOCTEXT("AlignToNormalTooltip",
			"Align the pawn to the ground normal under the cursor (Space also toggles while placing).\n"
			"Выравнивать павна по нормали грунта под курсором (пробел тоже тоглит в режиме расстановки)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				FIluPlayHereSettings::SetAlignToNormal(!FIluPlayHereSettings::GetAlignToNormal());
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return FIluPlayHereSettings::GetAlignToNormal(); })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FaceCameraYawLabel", "Face viewport camera"),
		LOCTEXT("FaceCameraYawTooltip",
			"Spawn the pawn heading the same way the viewport camera looks; the mouse wheel then rotates on top of that.\n"
			"Спавнить павна курсом по взгляду камеры вьюпорта; колесо мыши крутит поверх этого."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				FIluPlayHereSettings::SetFaceCameraYaw(!FIluPlayHereSettings::GetFaceCameraYaw());
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return FIluPlayHereSettings::GetFaceCameraYaw(); })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("IluPlaceActorOptions", LOCTEXT("PlaceActorOptionsSection", "Place Actor"));

	// Класс размещаемого актора — любой Actor-BP проекта.
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(220.f)
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(AActor::StaticClass())
			.AllowNone(true)
			.AllowAbstract(false)
			.ShowTreeView(false)
			.ToolTipText(LOCTEXT("PlaceActorClassTooltip",
				"Actor class (usually a Blueprint) placed by the Place Actor button.\n"
				"Класс актора (обычно Blueprint), который ставит кнопка Place Actor."))
			.SelectedClass_Static(&FIluPlaceActorSettings::GetActorClassForPicker)
			.OnSetClass_Static(&FIluPlaceActorSettings::SetActorClass)
		],
		LOCTEXT("PlaceActorClassLabel", "Actor class"));

	// Имя переменной варианта, которую листает Z (int/byte/enum).
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(140.f)
		[
			SNew(SEditableTextBox)
			.ToolTipText(LOCTEXT("VariantPropertyTooltip",
				"Integer (int/byte/enum) variable of the actor that Z cycles while placing, e.g. Type. Empty — no variants.\n"
				"Целочисленная (int/byte/enum) переменная актора, которую листает Z в режиме расстановки, например Type. Пусто — без вариантов."))
			.Text_Lambda([]() { return FText::FromName(FIluPlaceActorSettings::GetVariantProperty()); })
			.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
			{
				FIluPlaceActorSettings::SetVariantProperty(FName(*Text.ToString().TrimStartAndEnd()));
			})
		],
		LOCTEXT("VariantPropertyLabel", "Variant variable"));

	// Сколько вариантов листает Z: 0..N-1.
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(100.f)
		[
			SNew(SSpinBox<int32>)
			.MinValue(1)
			.MaxValue(FIluPlaceActorSettings::VariantCountMax)
			.Delta(1)
			.ToolTipText(LOCTEXT("VariantCountTooltip",
				"How many variants Z cycles through: values 0..N-1.\n"
				"Сколько вариантов листает Z: значения 0..N-1."))
			.Value_Static(&FIluPlaceActorSettings::GetVariantCount)
			.OnValueChanged_Static(&FIluPlaceActorSettings::SetVariantCount)
		],
		LOCTEXT("VariantCountLabel", "Variant count"));

	// Отдельный отступ по высоте для Place Actor (не связан с Play Here).
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(100.f)
		[
			SNew(SSpinBox<float>)
			.MinValue(-FIluPlaceActorSettings::LiftMaxAbsCm)
			.MaxValue(FIluPlaceActorSettings::LiftMaxAbsCm)
			.Delta(1.f)
			.ToolTipText(LOCTEXT("PlaceActorLiftTooltip",
				"Base spawn height offset for Place Actor, cm; separate from Play Here. Ctrl+wheel while placing adds on top.\n"
				"Базовый отступ по высоте для Place Actor, см; отдельный от Play Here. Ctrl+колесо в режиме расстановки — поверх него."))
			.Value_Static(&FIluPlaceActorSettings::GetDefaultLiftCm)
			.OnValueChanged_Static(&FIluPlaceActorSettings::SetDefaultLiftCm)
		],
		LOCTEXT("PlaceActorLiftLabel", "Default lift (cm)"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("PlaceActorAlignLabel", "Align to surface normal"),
		LOCTEXT("PlaceActorAlignTooltip",
			"Align the placed actor to the ground normal under the cursor; separate from Play Here (Space also toggles while placing).\n"
			"Выравнивать актор по нормали грунта под курсором; отдельно от Play Here (пробел тоже тоглит в режиме расстановки)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				FIluPlaceActorSettings::SetAlignToNormal(!FIluPlaceActorSettings::GetAlignToNormal());
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return FIluPlaceActorSettings::GetAlignToNormal(); })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("IluTimeOfDayOptions", LOCTEXT("TimeOfDayOptionsSection", "Time Of Day"));

	// Класс системы неба для слайдера Time Of Day на тулбаре вьюпорта.
	MenuBuilder.AddWidget(
		SNew(SBox)
		.Padding(FMargin(8.f, 2.f))
		.MinDesiredWidth(220.f)
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(AActor::StaticClass())
			.AllowNone(true)
			.AllowAbstract(false)
			.ShowTreeView(false)
			.ToolTipText(LOCTEXT("SkyClassTooltip",
				"Sky system Blueprint (Ultra Dynamic Sky) whose \"Time of Day\" variable the viewport slider drives. None — auto-detect an Ultra_Dynamic_Sky actor in the level.\n"
				"Blueprint системы неба (Ultra Dynamic Sky), чью переменную «Time of Day» крутит слайдер на тулбаре вьюпорта. None — автопоиск актора Ultra_Dynamic_Sky на уровне."))
			.SelectedClass_Static(&FIluTimeOfDaySettings::GetSkyClassForPicker)
			.OnSetClass_Static(&FIluTimeOfDaySettings::SetSkyClass)
		],
		LOCTEXT("SkyClassLabel", "Sky class"));

	MenuBuilder.EndSection();

	FIluMeshOpsMenu::AddSection(MenuBuilder);
	return MenuBuilder.MakeWidget();
}

void FIluUnrealToolDeveloperModule::TogglePlayHereTool()
{
	if (IsPlayHereToolActive())
	{
		PlayHereTool->RequestStop();
		PlayHereTool.Reset();
		return;
	}

	// Два режима расстановки одновременно не живут — Place Actor сворачиваем.
	if (IsPlaceActorToolActive())
	{
		PlaceActorTool->RequestStop();
		PlaceActorTool.Reset();
	}

	PlayHereTool = MakeShared<FIluPlayHereTool>();
	if (!PlayHereTool->Start())
	{
		PlayHereTool.Reset();
	}
}

bool FIluUnrealToolDeveloperModule::IsPlayHereToolActive() const
{
	return PlayHereTool.IsValid() && PlayHereTool->IsActive();
}

void FIluUnrealToolDeveloperModule::TogglePlaceActorTool()
{
	if (IsPlaceActorToolActive())
	{
		PlaceActorTool->RequestStop();
		PlaceActorTool.Reset();
		return;
	}

	// Два режима расстановки одновременно не живут — Play Here сворачиваем.
	if (IsPlayHereToolActive())
	{
		PlayHereTool->RequestStop();
		PlayHereTool.Reset();
	}

	PlaceActorTool = MakeShared<FIluPlaceActorTool>();
	if (!PlaceActorTool->Start())
	{
		PlaceActorTool.Reset();
	}
}

bool FIluUnrealToolDeveloperModule::IsPlaceActorToolActive() const
{
	return PlaceActorTool.IsValid() && PlaceActorTool->IsActive();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FIluUnrealToolDeveloperModule, IluUnrealToolDeveloper)
