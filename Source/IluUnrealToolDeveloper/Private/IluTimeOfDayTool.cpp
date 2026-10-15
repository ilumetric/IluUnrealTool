#include "IluTimeOfDayTool.h"
#include "IluUnrealToolCore.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/AppStyle.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "IluUnrealTool"

namespace IluTimeOfDay
{
	// Persistent settings / Сохраняемые настройки.
	static const TCHAR* SettingsSection = TEXT("IluUnrealTool.TimeOfDay");
	static FString GSkyClassPath;
	static bool GSettingsLoaded = false;

	// Loaded class for the current path / Загруженный класс для текущего пути.
	static TWeakObjectPtr<UClass> GSkyClassCache;

	// Sky actor found in the world; re-resolved when the world or the actor dies.
	// Найденный актор неба; переискивается, когда мир или актор умирает.
	static TWeakObjectPtr<AActor> GSkyActorCache;

	// Last value pushed/read — the wheel falls back to it when no sky actor is around.
	// Последнее записанное/прочитанное значение — фолбэк колеса, когда актора неба нет.
	static float GLastTime = 1200.f;

	static void LoadSettings()
	{
		if (GSettingsLoaded || !GConfig)
		{
			return;
		}
		GSettingsLoaded = true;
		GConfig->GetString(SettingsSection, TEXT("SkyClass"), GSkyClassPath, GEditorPerProjectIni);
	}

	// Active world: PIE while playing, otherwise the editor world.
	// Активный мир: PIE во время игры, иначе мир редактора.
	static UWorld* GetActiveWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		if (GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	// The "Time of Day" float/double variable of a BP class: match by name with spaces
	// stripped, case-insensitive — BP variable names may keep their spaces.
	// Переменная «Time of Day» (float/double) BP-класса: сравниваем имя без пробелов,
	// без учёта регистра — имена BP-переменных могут содержать пробелы.
	static FProperty* FindTimeOfDayProperty(const UClass* Class)
	{
		if (!Class)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop->IsA<FFloatProperty>() && !Prop->IsA<FDoubleProperty>())
			{
				continue;
			}
			FString Name = Prop->GetName();
			Name.ReplaceInline(TEXT(" "), TEXT(""));
			if (Name.Equals(TEXT("TimeOfDay"), ESearchCase::IgnoreCase))
			{
				return Prop;
			}
		}
		return nullptr;
	}

	static bool ReadTime(const AActor* Actor, float& OutTime)
	{
		const FProperty* Prop = Actor ? FindTimeOfDayProperty(Actor->GetClass()) : nullptr;
		if (!Prop)
		{
			return false;
		}
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
		if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			OutTime = float(DoubleProp->GetPropertyValue(ValuePtr));
			return true;
		}
		if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			OutTime = FloatProp->GetPropertyValue(ValuePtr);
			return true;
		}
		return false;
	}

	static bool WriteTime(AActor* Actor, float Time)
	{
		const FProperty* Prop = Actor ? FindTimeOfDayProperty(Actor->GetClass()) : nullptr;
		if (!Prop)
		{
			return false;
		}
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
		if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue(ValuePtr, double(Time));
			return true;
		}
		if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue(ValuePtr, Time);
			return true;
		}
		return false;
	}

	// Days are a circle: 2400 wraps back to 0, so the wheel never hits a stop.
	// Сутки — круг: 2400 заворачивается в 0, поэтому у колеса нет упоров.
	static float WrapTime(float Time)
	{
		constexpr float Range = FIluTimeOfDayTool::TimeMax - FIluTimeOfDayTool::TimeMin;
		float Wrapped = FMath::Fmod(Time - FIluTimeOfDayTool::TimeMin, Range);
		if (Wrapped < 0.f)
		{
			Wrapped += Range;
		}
		return FIluTimeOfDayTool::TimeMin + Wrapped;
	}

	// UDS time 0..2400 is fractional hours x100 (960 = 9:36) / Время UDS — доли часа x100.
	static FText FormatTime(float Time)
	{
		const float Hours = WrapTime(Time) / 100.f;
		int32 H = FMath::FloorToInt32(Hours);
		int32 M = FMath::RoundToInt32((Hours - float(H)) * 60.f);
		if (M >= 60)
		{
			M -= 60;
			++H;
		}
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), H % 24, M));
	}
}

UClass* FIluTimeOfDaySettings::ResolveSkyClass()
{
	IluTimeOfDay::LoadSettings();
	if (UClass* Cached = IluTimeOfDay::GSkyClassCache.Get())
	{
		return Cached;
	}
	if (IluTimeOfDay::GSkyClassPath.IsEmpty())
	{
		return nullptr;
	}
	UClass* Loaded = FSoftClassPath(IluTimeOfDay::GSkyClassPath).TryLoadClass<AActor>();
	IluTimeOfDay::GSkyClassCache = Loaded;
	if (!Loaded)
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("TimeOfDay: класс неба «%s» не загрузился"), *IluTimeOfDay::GSkyClassPath);
	}
	return Loaded;
}

const UClass* FIluTimeOfDaySettings::GetSkyClassForPicker()
{
	return ResolveSkyClass();
}

void FIluTimeOfDaySettings::SetSkyClass(const UClass* Class)
{
	IluTimeOfDay::LoadSettings();
	IluTimeOfDay::GSkyClassCache = const_cast<UClass*>(Class);
	IluTimeOfDay::GSkyClassPath = Class ? FSoftClassPath(Class).ToString() : FString();
	IluTimeOfDay::GSkyActorCache.Reset();
	if (GConfig)
	{
		GConfig->SetString(IluTimeOfDay::SettingsSection, TEXT("SkyClass"), *IluTimeOfDay::GSkyClassPath, GEditorPerProjectIni);
	}
	UE_LOG(LogIluUnrealTool, Log, TEXT("TimeOfDay: выбран класс неба «%s»"), Class ? *Class->GetName() : TEXT("автопоиск"));
}

AActor* FIluTimeOfDayTool::ResolveSkyActor()
{
	UWorld* World = IluTimeOfDay::GetActiveWorld();
	if (!World)
	{
		return nullptr;
	}

	// Cache is only valid for the active world / Кэш валиден только для активного мира.
	if (AActor* Cached = IluTimeOfDay::GSkyActorCache.Get())
	{
		if (Cached->GetWorld() == World)
		{
			return Cached;
		}
	}

	AActor* Found = nullptr;
	if (UClass* SkyClass = FIluTimeOfDaySettings::ResolveSkyClass())
	{
		for (TActorIterator<AActor> It(World, SkyClass); It; ++It)
		{
			Found = *It;
			break;
		}
	}
	else
	{
		// Auto-detect: UDS Blueprint class is named Ultra_Dynamic_Sky(_C).
		// Автопоиск: класс Blueprint'а UDS называется Ultra_Dynamic_Sky(_C).
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(TEXT("Ultra_Dynamic_Sky")))
			{
				Found = *It;
				break;
			}
		}
	}

	IluTimeOfDay::GSkyActorCache = Found;
	return Found;
}

float FIluTimeOfDayTool::GetTimeOfDay()
{
	float Time = IluTimeOfDay::GLastTime;
	if (IluTimeOfDay::ReadTime(ResolveSkyActor(), Time))
	{
		IluTimeOfDay::GLastTime = Time;
	}
	return Time;
}

void FIluTimeOfDayTool::SetTimeOfDay(float Time)
{
	Time = IluTimeOfDay::WrapTime(Time);
	IluTimeOfDay::GLastTime = Time;

	AActor* Sky = ResolveSkyActor();
	if (!Sky)
	{
		return;
	}

	const bool bEditorWorld = GEditor && GEditor->PlayWorld == nullptr;
	if (bEditorWorld)
	{
		// No transaction (a drag would spam undo), but the level must know it changed.
		// Без транзакции (драг заспамил бы undo), но уровень должен узнать об изменении.
		Sky->Modify();
	}

	if (!IluTimeOfDay::WriteTime(Sky, Time))
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("TimeOfDay: у «%s» нет float/double переменной Time of Day"), *Sky->GetClass()->GetName());
		return;
	}

	if (bEditorWorld)
	{
		// UDS updates the sun from its construction script in the editor.
		// В редакторе UDS обновляет солнце из construction script'а.
		Sky->RerunConstructionScripts();
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}
	// In PIE the UDS tick picks the new value up by itself / В PIE тик UDS сам подхватит значение.
}

namespace IluTimeOfDay
{
	// Wheel geometry / Геометрия колеса.
	static constexpr float WheelWidth = 132.f;
	static constexpr float WheelHeight = 20.f;
	// Drag scale: how many pixels one hour takes / Масштаб драга: сколько пикселей на час.
	static constexpr float PixelsPerHour = 46.f;
	static constexpr float PixelsPerUnit = PixelsPerHour / 100.f;
	// Tick spacing: 15 minutes, every 4th is an hour, every 96th is midnight.
	// Шаг рисок: 15 минут, каждая 4-я — час, каждая 96-я — полночь.
	static constexpr float TickStep = 25.f;
	// Mouse wheel notch / Щелчок колеса мыши — 15 минут.
	static constexpr float WheelNotch = 25.f;
}

/**
 * Endless Time Of Day wheel: horizontal drag scrolls the day with no end stops (2400 wraps to 0),
 * ticks scroll under a fixed centre marker, the exact time is printed next to it.
 * Shift — x4 faster, Ctrl — x4 finer, mouse wheel — 15 minutes per notch.
 * Бесконечное колесо Time Of Day: горизонтальный драг крутит сутки без упоров (2400 → 0),
 * риски едут под неподвижной центральной меткой, точное время печатается рядом.
 * Shift — в 4 раза быстрее, Ctrl — в 4 раза точнее, колесо мыши — 15 минут за щелчок.
 */
class SIluTimeOfDayWheel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SIluTimeOfDayWheel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetCursor(EMouseCursor::ResizeLeftRight);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(IluTimeOfDay::WheelWidth, IluTimeOfDay::WheelHeight);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
		{
			return FReply::Unhandled();
		}
		// Remember where the cursor was: high precision mode hides and pins it for the drag.
		// Запоминаем позицию курсора: режим high precision прячет и удерживает его на время драга.
		CachedMousePosition = MouseEvent.GetScreenSpacePosition().IntPoint();
		bDragging = true;
		return FReply::Handled()
			.CaptureMouse(SharedThis(this))
			.UseHighPrecisionMouseMovement(SharedThis(this))
			.PreventThrottling();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bDragging)
		{
			return FReply::Unhandled();
		}
		bDragging = false;
		// Put the cursor back where the drag started / Возвращаем курсор туда, откуда начали тянуть.
		return FReply::Handled().ReleaseMouseCapture().SetMousePos(CachedMousePosition);
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!bDragging || !HasMouseCapture())
		{
			return FReply::Unhandled();
		}
		const float DeltaPx = float(MouseEvent.GetCursorDelta().X) / FMath::Max(MyGeometry.Scale, KINDA_SMALL_NUMBER);
		FIluTimeOfDayTool::SetTimeOfDay(FIluTimeOfDayTool::GetTimeOfDay() + DeltaPx / IluTimeOfDay::PixelsPerUnit * SpeedScale(MouseEvent));
		return FReply::Handled();
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!IsEnabled())
		{
			return FReply::Unhandled();
		}
		FIluTimeOfDayTool::SetTimeOfDay(FIluTimeOfDayTool::GetTimeOfDay() + MouseEvent.GetWheelDelta() * IluTimeOfDay::WheelNotch * SpeedScale(MouseEvent));
		return FReply::Handled();
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const bool bEnabled = ShouldBeEnabled(bParentEnabled);
		const float Dim = bEnabled ? 1.f : 0.35f;
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FVector2f Size = AllottedGeometry.GetLocalSize();
		const float Half = Size.X * 0.5f;
		const float MidY = Size.Y * 0.5f;
		const float Time = IluTimeOfDay::WrapTime(FIluTimeOfDayTool::GetTimeOfDay());

		// Backdrop tinted by daylight: dark at night, cool grey at noon.
		// Подложка, подкрашенная светом суток: тёмная ночью, холодно-серая в полдень.
		const float Daylight = FMath::SmoothStep(400.f, 800.f, Time) * (1.f - FMath::SmoothStep(1700.f, 2100.f, Time));
		const FLinearColor Backdrop = FMath::Lerp(FLinearColor(0.015f, 0.02f, 0.045f, 0.85f), FLinearColor(0.09f, 0.10f, 0.12f, 0.85f), Daylight) * Dim;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, Backdrop);

		// Ticks scrolling under the centre / Риски, едущие под центром.
		const float VisibleUnits = Half / IluTimeOfDay::PixelsPerUnit;
		const int32 First = FMath::CeilToInt((Time - VisibleUnits) / IluTimeOfDay::TickStep);
		const int32 Last = FMath::FloorToInt((Time + VisibleUnits) / IluTimeOfDay::TickStep);
		const int32 TickLayer = LayerId + 1;
		for (int32 Index = First; Index <= Last; ++Index)
		{
			const float X = Half + (float(Index) * IluTimeOfDay::TickStep - Time) * IluTimeOfDay::PixelsPerUnit;
			// Fade towards the edges so the wheel reads as endless / Затухание к краям — колесо читается бесконечным.
			const float Edge = FMath::Clamp(FMath::Abs(X - Half) / Half, 0.f, 1.f);
			const float Fade = FMath::Clamp(1.f - Edge * Edge, 0.f, 1.f) * Dim;
			const bool bMidnight = FMath::Abs(Index) % 96 == 0;
			const bool bHour = Index % 4 == 0;
			const float TickHalf = bMidnight ? MidY - 2.f : (bHour ? 5.f : 2.5f);
			const FLinearColor TickColor = bMidnight
				? FLinearColor(0.45f, 0.65f, 1.f, Fade)
				: FLinearColor(1.f, 1.f, 1.f, Fade * (bHour ? 0.75f : 0.35f));

			TArray<FVector2D> Points;
			Points.Add(FVector2D(X, MidY - TickHalf));
			Points.Add(FVector2D(X, MidY + TickHalf));
			FSlateDrawElement::MakeLines(OutDrawElements, TickLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, TickColor, false, 1.f);
		}

		// Fixed centre marker / Неподвижная центральная метка.
		TArray<FVector2D> Marker;
		Marker.Add(FVector2D(Half, 1.f));
		Marker.Add(FVector2D(Half, Size.Y - 1.f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Marker, ESlateDrawEffect::None,
			FLinearColor(1.f, 0.78f, 0.35f, Dim), false, 2.f);

		return LayerId + 2;
	}

private:
	// Shift accelerates, Ctrl refines / Shift ускоряет, Ctrl уточняет.
	static float SpeedScale(const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.IsShiftDown())
		{
			return 4.f;
		}
		if (MouseEvent.IsControlDown())
		{
			return 0.25f;
		}
		return 1.f;
	}

	bool bDragging = false;
	FIntPoint CachedMousePosition = FIntPoint::ZeroValue;
};

TSharedRef<SWidget> FIluTimeOfDayTool::MakeToolbarWidget()
{
	const TAttribute<FText> TooltipAttr = TAttribute<FText>::CreateLambda([]()
	{
		if (const AActor* Sky = ResolveSkyActor())
		{
			return FText::Format(LOCTEXT("TodTooltipFound",
				"Time Of Day: {0} ({1}) — drives \"{2}\" in the level. Drag left/right to spin the day endlessly,\n"
				"mouse wheel — 15 min, Shift — faster, Ctrl — finer. Sky class is set in the \"...\" panel.\n"
				"Time Of Day: {0} ({1}) — крутит «{2}» на уровне. Тяните влево/вправо — сутки крутятся бесконечно,\n"
				"колесо мыши — 15 мин, Shift — быстрее, Ctrl — точнее. Класс неба задаётся в подпанели «...»."),
				IluTimeOfDay::FormatTime(GetTimeOfDay()),
				FText::AsNumber(FMath::RoundToInt32(GetTimeOfDay())),
				FText::FromString(Sky->GetActorNameOrLabel()));
		}
		return LOCTEXT("TodTooltipMissing",
			"No sky actor in the level (Ultra Dynamic Sky). Pick the sky class in the \"...\" panel next to Play Here or drop a UDS actor into the level.\n"
			"Актора неба на уровне нет (Ultra Dynamic Sky). Выберите класс неба в подпанели «...» рядом с Play Here или добавьте UDS на уровень.");
	});

	const TAttribute<bool> EnabledAttr = TAttribute<bool>::CreateLambda([]()
	{
		return ResolveSkyActor() != nullptr;
	});

	return SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(6.f, 0.f, 4.f, 0.f))
		.ToolTipText(TooltipAttr)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SIluTimeOfDayWheel)
				.IsEnabled(EnabledAttr)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox)
				.MinDesiredWidth(38.f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text_Lambda([]() { return IluTimeOfDay::FormatTime(GetTimeOfDay()); })
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
