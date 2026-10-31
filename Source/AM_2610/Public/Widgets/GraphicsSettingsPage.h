#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericWindow.h"
#include "Utility/BaseGameInstance.h" // FGraphicsSettings
#include "GraphicsSettingsPage.generated.h"

class UComboBoxString;
class UCheckBox;
class USlider;
class UButtonWithText;
class UTextBlock;
class USizeBox;
class UWidget;

/**
 * Graphics page for the settings menu (WidgetSwitcher page 2).
 * Resolution / window mode / VSync / quality preset / brightness / FPS limit.
 * A monitor-frame preview resizes to the pending resolution's aspect ratio, and a
 * "keep these settings?" countdown rolls back a bad display mode.
 *
 * All values are owned by UBaseGameInstance (GetGraphicsSettings / ApplyGraphics / CommitGraphics).
 * This page only stages values.
 *
 * Nothing is applied until Apply is pressed; nothing is written to disk until Keep
 * (or immediately, for changes that can't strand the player).
 */
UCLASS()
class AM_2610_API UGraphicsSettingsPage : public UUserWidget
{
	GENERATED_BODY()

public:
	/** True while the "keep these settings?" overlay is up. SettingsMenu uses this to lock tabs. */
	bool IsAwaitingConfirm() const { return bAwaitingConfirm; }

	/** Rolls back to the pre-Apply settings if a confirmation is pending (no-op otherwise). */
	void CancelPendingChanges();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ---- Bound widgets (names must match in WBP_GraphicsSettingsPage) ----
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UComboBoxString* ResolutionCombo;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UComboBoxString* WindowModeCombo;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UCheckBox* VSyncCheck;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UComboBoxString* QualityCombo;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UComboBoxString* FPSLimitCombo;
	/** Configure 0..1 in the Designer (Step Size 0.05 feels good). 0.5 = engine default brightness. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) USlider* BrightnessSlider;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButtonWithText* ApplyButton;

	/** Monitor "screen" - put your frame image/border inside this SizeBox. It gets resized. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) USizeBox* PreviewSizeBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* ResolutionLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* AspectLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* BrightnessLabel;

	/** Overlay shown during the revert countdown (dimmer + text + two buttons). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UWidget* ConfirmPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* ConfirmText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButtonWithText* KeepButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButtonWithText* RevertButton;

	// ---- Tuning ----
	/** Largest the preview can be (px), before the resolution-relative scaling. */
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FVector2D PreviewMaxSize = FVector2D(360.0, 220.0);

	/** Preview scale at the lowest listed resolution (1.0 at the highest). Makes same-aspect changes visible. */
	UPROPERTY(EditDefaultsOnly, Category = "Preview", meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float MinPreviewScale = 0.55f;

	/** How fast the preview animates toward its new size. */
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	float PreviewInterpSpeed = 10.f;

	/** Hide resolutions narrower than this. */
	UPROPERTY(EditDefaultsOnly, Category = "Graphics")
	int32 MinListedWidth = 1024;

	UPROPERTY(EditDefaultsOnly, Category = "Graphics")
	int32 RevertSeconds = 15;

private:
	// UI callbacks (these only stage values)
	UFUNCTION() void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleFPSLimitChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleVSyncChanged(bool bIsChecked);
	UFUNCTION() void HandleBrightnessChanged(float Value);
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleKeepClicked();
	UFUNCTION() void HandleRevertClicked();

	// Setup
	void PopulateResolutions();
	void PopulateWindowModes();
	void PopulateQuality();
	void PopulateFrameLimits();
	void LoadFromSettings();

	// Preview / labels
	void UpdatePreviewTarget(bool bSnap = false);
	void UpdateLabels();
	void UpdateBrightnessLabel();
	void RefreshApplyState();
	static FString ResolutionToString(const FIntPoint& Res);
	static FString AspectToString(const FIntPoint& Res);

	// Revert countdown (driven by NativeTick: world timers freeze while the game is paused)
	void StartRevertCountdown();
	void UpdateConfirmText();
	void RestorePreviousSettings();
	void HideConfirmPanel();

	bool HasPendingChanges() const;
	UBaseGameInstance* GetGI() const { return GetGameInstance<UBaseGameInstance>(); }

	// Options (parallel to combo box indices)
	TArray<FIntPoint> Resolutions;
	TArray<TEnumAsByte<EWindowMode::Type>> WindowModes;
	TArray<float> FrameLimits; // 0 = unlimited
	int32 MinResWidth = 0;
	int32 MaxResWidth = 0;

	// Staged values (not applied until Apply)
	FGraphicsSettings Pending;

	// Snapshot taken at Apply, restored on revert
	FGraphicsSettings Prev;

	// Preview animation
	FVector2D CurrentPreviewSize = FVector2D::ZeroVector;
	FVector2D TargetPreviewSize = FVector2D::ZeroVector;

	// Countdown
	bool bAwaitingConfirm = false;
	float RevertTimeLeft = 0.f;
	int32 LastShownSeconds = -1;
};