#include "Widgets/GraphicsSettingsPage.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Both/ButtonWithText.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/UnrealMathUtility.h"

namespace
{
	constexpr int32 NumQualityLevels = 5; // Low, Medium, High, Epic, Cinematic
}

// ------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------

void UGraphicsSettingsPage::NativeConstruct()
{
	Super::NativeConstruct();

	// AddUnique so re-adding the widget to the viewport doesn't double-bind.
	ResolutionCombo->OnSelectionChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleResolutionChanged);
	WindowModeCombo->OnSelectionChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleWindowModeChanged);
	QualityCombo->OnSelectionChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleQualityChanged);
	FPSLimitCombo->OnSelectionChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleFPSLimitChanged);
	VSyncCheck->OnCheckStateChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleVSyncChanged);
	RenderScaleSlider->OnValueChanged.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleRenderScaleChanged);
	ApplyButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleApplyClicked);
	KeepButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleKeepClicked);
	RevertButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UGraphicsSettingsPage::HandleRevertClicked);

	bAwaitingConfirm = false;

	PopulateResolutions();
	PopulateWindowModes();
	PopulateQuality();
	PopulateFrameLimits();
	LoadFromSettings();
	HideConfirmPanel();
}

void UGraphicsSettingsPage::NativeDestruct()
{
	// If the page goes away mid-countdown, treat it as "not confirmed" and roll back.
	CancelPendingChanges();
	Super::NativeDestruct();
}

void UGraphicsSettingsPage::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Slate ticks with real time, so both of these keep running while the game is paused
	// (the pause menu opens this page).

	if (PreviewSizeBox)
	{
		CurrentPreviewSize = FMath::Vector2DInterpTo(CurrentPreviewSize, TargetPreviewSize, InDeltaTime, PreviewInterpSpeed);
		PreviewSizeBox->SetWidthOverride(static_cast<float>(CurrentPreviewSize.X));
		PreviewSizeBox->SetHeightOverride(static_cast<float>(CurrentPreviewSize.Y));
	}

	if (bAwaitingConfirm)
	{
		// Clamp so the hitch from a window-mode switch can't eat the whole countdown.
		RevertTimeLeft -= FMath::Min(InDeltaTime, 0.25f);
		if (RevertTimeLeft <= 0.f)
		{
			RestorePreviousSettings();
		}
		else
		{
			const int32 Secs = FMath::CeilToInt(RevertTimeLeft);
			if (Secs != LastShownSeconds)
			{
				LastShownSeconds = Secs;
				UpdateConfirmText();
			}
		}
	}
}

void UGraphicsSettingsPage::CancelPendingChanges()
{
	if (bAwaitingConfirm)
	{
		RestorePreviousSettings();
	}
}

// ------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------

void UGraphicsSettingsPage::PopulateResolutions()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();

	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);

	Resolutions.Reset();
	for (const FIntPoint& Res : Supported)
	{
		if (Res.X >= MinListedWidth)
		{
			Resolutions.AddUnique(Res);
		}
	}

	// Make sure whatever is currently active is selectable, even if it's unusual.
	Resolutions.AddUnique(GUS->GetScreenResolution());

	// Highest first.
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return (A.X != B.X) ? (A.X > B.X) : (A.Y > B.Y);
		});

	MaxResWidth = Resolutions.Num() > 0 ? Resolutions[0].X : 1920;
	MinResWidth = Resolutions.Num() > 0 ? Resolutions.Last().X : 1920;

	ResolutionCombo->ClearOptions();
	for (const FIntPoint& Res : Resolutions)
	{
		ResolutionCombo->AddOption(ResolutionToString(Res));
	}
}

void UGraphicsSettingsPage::PopulateWindowModes()
{
	WindowModes = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	WindowModeCombo->ClearOptions();
	WindowModeCombo->AddOption(TEXT("Fullscreen"));
	WindowModeCombo->AddOption(TEXT("Windowed Fullscreen"));
	WindowModeCombo->AddOption(TEXT("Windowed"));
}

void UGraphicsSettingsPage::PopulateQuality()
{
	// Index == Scalability level, so no lookup table needed.
	QualityCombo->ClearOptions();
	QualityCombo->AddOption(TEXT("Low"));
	QualityCombo->AddOption(TEXT("Medium"));
	QualityCombo->AddOption(TEXT("High"));
	QualityCombo->AddOption(TEXT("Epic"));
	QualityCombo->AddOption(TEXT("Cinematic"));
}

void UGraphicsSettingsPage::PopulateFrameLimits()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();

	FrameLimits = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f, 0.f };
	FrameLimits.AddUnique(GUS->GetFrameRateLimit()); // keep an odd saved value selectable

	// Ascending, with 0 (= unlimited) last.
	FrameLimits.Sort([](float A, float B)
		{
			const float a = (A <= 0.f) ? TNumericLimits<float>::Max() : A;
			const float b = (B <= 0.f) ? TNumericLimits<float>::Max() : B;
			return a < b;
		});

	FPSLimitCombo->ClearOptions();
	for (const float Limit : FrameLimits)
	{
		FPSLimitCombo->AddOption(Limit <= 0.f
			? FString(TEXT("Unlimited"))
			: FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Limit)));
	}
}

void UGraphicsSettingsPage::LoadFromSettings()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();

	// Set the staged values first, then the widgets (their change events just re-stage the same values).
	PendingRes = GUS->GetScreenResolution();
	PendingMode = GUS->GetFullscreenMode();
	bPendingVSync = GUS->IsVSyncEnabled();
	PendingQuality = GUS->GetOverallScalabilityLevel(); // -1 if the groups are mixed
	PendingRenderScale = GUS->GetResolutionScaleNormalized();
	PendingFPS = GUS->GetFrameRateLimit();

	const int32 ResIndex = Resolutions.IndexOfByKey(PendingRes);
	ResolutionCombo->SetSelectedIndex(ResIndex != INDEX_NONE ? ResIndex : 0);

	const int32 ModeIndex = WindowModes.IndexOfByKey(PendingMode);
	WindowModeCombo->SetSelectedIndex(ModeIndex != INDEX_NONE ? ModeIndex : 0);

	VSyncCheck->SetIsChecked(bPendingVSync);

	if (PendingQuality >= 0 && PendingQuality < NumQualityLevels)
	{
		QualityCombo->SetSelectedIndex(PendingQuality);
	}
	else
	{
		QualityCombo->ClearSelection(); // custom / mixed settings: show nothing selected
	}

	RenderScaleSlider->SetValue(PendingRenderScale);

	const int32 FPSIndex = FrameLimits.IndexOfByPredicate([this](float L) { return FMath::IsNearlyEqual(L, PendingFPS, 0.5f); });
	FPSLimitCombo->SetSelectedIndex(FPSIndex != INDEX_NONE ? FPSIndex : FrameLimits.Num() - 1);

	UpdatePreviewTarget(/*bSnap=*/true);
	UpdateLabels();
	UpdateRenderScaleLabel();
	RefreshApplyState();
}

// ------------------------------------------------------------------
// UI callbacks (these only stage values; nothing is applied yet)
// ------------------------------------------------------------------

void UGraphicsSettingsPage::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = ResolutionCombo->GetSelectedIndex();
	if (Resolutions.IsValidIndex(Index))
	{
		PendingRes = Resolutions[Index];
	}

	UpdatePreviewTarget();
	UpdateLabels();
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = WindowModeCombo->GetSelectedIndex();
	if (WindowModes.IsValidIndex(Index))
	{
		PendingMode = WindowModes[Index];
	}
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = QualityCombo->GetSelectedIndex();
	if (Index >= 0 && Index < NumQualityLevels)
	{
		PendingQuality = Index;
	}
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleFPSLimitChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = FPSLimitCombo->GetSelectedIndex();
	if (FrameLimits.IsValidIndex(Index))
	{
		PendingFPS = FrameLimits[Index];
	}
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleVSyncChanged(bool bIsChecked)
{
	bPendingVSync = bIsChecked;
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleRenderScaleChanged(float Value)
{
	PendingRenderScale = Value;
	UpdateRenderScaleLabel();
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleApplyClicked()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();

	// Snapshot everything so a revert restores the full pre-Apply state.
	PrevRes = GUS->GetScreenResolution();
	PrevMode = GUS->GetFullscreenMode();
	bPrevVSync = GUS->IsVSyncEnabled();
	PrevQuality = GUS->GetOverallScalabilityLevel();
	PrevRenderScale = GUS->GetResolutionScaleNormalized();
	PrevFPS = GUS->GetFrameRateLimit();

	const bool bDisplayChanged = (PendingRes != PrevRes) || (PendingMode != PrevMode);

	GUS->SetScreenResolution(PendingRes);
	GUS->SetFullscreenMode(PendingMode);
	GUS->SetVSyncEnabled(bPendingVSync);
	GUS->SetFrameRateLimit(PendingFPS);

	// Order matters: a preset also resets render scale to its own value, so the slider is applied after it and wins.
	if (PendingQuality >= 0 && PendingQuality != PrevQuality)
	{
		GUS->SetOverallScalabilityLevel(PendingQuality);
	}
	GUS->SetResolutionScaleNormalized(PendingRenderScale);

	// Split apply (instead of ApplySettings) so nothing is written to the ini until the player confirms.
	GUS->ApplyNonResolutionSettings();
	GUS->ApplyResolutionSettings(false);

	if (bDisplayChanged)
	{
		StartRevertCountdown();
	}
	else
	{
		// Quality / scale / FPS / VSync can't strand the player, so no confirmation needed.
		GUS->SaveSettings();
	}

	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleKeepClicked()
{
	UGameUserSettings::GetGameUserSettings()->SaveSettings();
	HideConfirmPanel();
	RefreshApplyState();
}

void UGraphicsSettingsPage::HandleRevertClicked()
{
	RestorePreviousSettings();
}

// ------------------------------------------------------------------
// Preview + labels
// ------------------------------------------------------------------

void UGraphicsSettingsPage::UpdatePreviewTarget(bool bSnap)
{
	const float Aspect = static_cast<float>(PendingRes.X) / static_cast<float>(FMath::Max(1, PendingRes.Y));

	// Fit the aspect ratio inside the max box.
	float W = static_cast<float>(PreviewMaxSize.X);
	float H = W / Aspect;
	if (H > PreviewMaxSize.Y)
	{
		H = static_cast<float>(PreviewMaxSize.Y);
		W = H * Aspect;
	}

	// Shrink lower resolutions so 1080p -> 720p is visible even at the same aspect.
	float Alpha = 1.f;
	if (MaxResWidth > MinResWidth)
	{
		Alpha = FMath::Clamp(static_cast<float>(PendingRes.X - MinResWidth) / static_cast<float>(MaxResWidth - MinResWidth), 0.f, 1.f);
	}
	const float Scale = FMath::Lerp(MinPreviewScale, 1.f, Alpha);

	TargetPreviewSize = FVector2D(W * Scale, H * Scale);

	if (bSnap)
	{
		CurrentPreviewSize = TargetPreviewSize;
	}
}

void UGraphicsSettingsPage::UpdateLabels()
{
	ResolutionLabel->SetText(FText::FromString(ResolutionToString(PendingRes)));
	AspectLabel->SetText(FText::FromString(AspectToString(PendingRes)));
}

void UGraphicsSettingsPage::UpdateRenderScaleLabel()
{
	if (!RenderScaleLabel)
	{
		return;
	}

	// Normalized 0..1 maps onto the engine's min..max screen percentage.
	float CurrentNormalized = 1.f, CurrentValue = 100.f, Min = 100.f, Max = 100.f;
	UGameUserSettings::GetGameUserSettings()->GetResolutionScaleInformationEx(CurrentNormalized, CurrentValue, Min, Max);
	const float Percent = FMath::Lerp(Min, Max, PendingRenderScale);

	RenderScaleLabel->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent))));
}

void UGraphicsSettingsPage::RefreshApplyState()
{
	ApplyButton->SetIsEnabled(HasPendingChanges() && !bAwaitingConfirm);
}

bool UGraphicsSettingsPage::HasPendingChanges() const
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	return PendingRes != GUS->GetScreenResolution()
		|| PendingMode != GUS->GetFullscreenMode()
		|| bPendingVSync != GUS->IsVSyncEnabled()
		|| (PendingQuality >= 0 && PendingQuality != GUS->GetOverallScalabilityLevel())
		|| !FMath::IsNearlyEqual(PendingRenderScale, GUS->GetResolutionScaleNormalized(), 0.005f)
		|| !FMath::IsNearlyEqual(PendingFPS, GUS->GetFrameRateLimit(), 0.5f);
}

FString UGraphicsSettingsPage::ResolutionToString(const FIntPoint& Res)
{
	return FString::Printf(TEXT("%d x %d"), Res.X, Res.Y);
}

FString UGraphicsSettingsPage::AspectToString(const FIntPoint& Res)
{
	struct FKnownAspect { float Value; const TCHAR* Name; };
	static const FKnownAspect Known[] =
	{
		{ 16.f / 9.f,  TEXT("16:9")  },
		{ 16.f / 10.f, TEXT("16:10") },
		{ 4.f / 3.f,   TEXT("4:3")   },
		{ 21.f / 9.f,  TEXT("21:9")  },
		{ 32.f / 9.f,  TEXT("32:9")  },
		{ 3.f / 2.f,   TEXT("3:2")   },
		{ 5.f / 4.f,   TEXT("5:4")   },
	};

	const float Aspect = static_cast<float>(Res.X) / static_cast<float>(FMath::Max(1, Res.Y));
	for (const FKnownAspect& K : Known)
	{
		if (FMath::Abs(Aspect - K.Value) < 0.03f)
		{
			return K.Name;
		}
	}

	// Fallback: reduced ratio.
	const int32 G = FMath::Max(1, FMath::GreatestCommonDivisor(Res.X, Res.Y));
	return FString::Printf(TEXT("%d:%d"), Res.X / G, Res.Y / G);
}

// ------------------------------------------------------------------
// Revert countdown
// ------------------------------------------------------------------

void UGraphicsSettingsPage::StartRevertCountdown()
{
	bAwaitingConfirm = true;
	RevertTimeLeft = static_cast<float>(RevertSeconds);
	LastShownSeconds = RevertSeconds;

	ConfirmPanel->SetVisibility(ESlateVisibility::Visible);
	UpdateConfirmText();
}

void UGraphicsSettingsPage::UpdateConfirmText()
{
	ConfirmText->SetText(FText::FromString(
		FString::Printf(TEXT("Keep these display settings? Reverting in %d..."), FMath::Max(1, FMath::CeilToInt(RevertTimeLeft)))));
}

void UGraphicsSettingsPage::RestorePreviousSettings()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	GUS->SetScreenResolution(PrevRes);
	GUS->SetFullscreenMode(PrevMode);
	GUS->SetVSyncEnabled(bPrevVSync);
	GUS->SetFrameRateLimit(PrevFPS);
	if (PrevQuality >= 0) // a custom/mixed preset can't be restored through the overall level
	{
		GUS->SetOverallScalabilityLevel(PrevQuality);
	}
	GUS->SetResolutionScaleNormalized(PrevRenderScale);
	GUS->ApplyNonResolutionSettings();
	GUS->ApplyResolutionSettings(false);

	HideConfirmPanel();
	LoadFromSettings(); // snap the controls and preview back
}

void UGraphicsSettingsPage::HideConfirmPanel()
{
	bAwaitingConfirm = false;
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
}