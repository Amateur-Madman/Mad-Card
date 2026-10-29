#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsMenu.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsMenuClosed);

// Shell with tab buttons + a WidgetSwitcher. Page 0 = Input, page 1 = Audio, page 2 = Graphics.
// Opened by the main menu and the pause menu; they listen to OnClosed to re-show themselves.
UCLASS()
class AM_2610_API USettingsMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingsMenuClosed OnClosed;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UWidgetSwitcher* PageSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* InputTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* AudioTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* GraphicsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* BackButton;

	// The graphics page instance inside PageSwitcher (name it GraphicsPage in the Designer).
	// Needed so tabs/Back can respect the "keep these settings?" countdown.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UGraphicsSettingsPage* GraphicsPage;

private:
	UFUNCTION() void HandleInputTabClicked();
	UFUNCTION() void HandleAudioTabClicked();
	UFUNCTION() void HandleGraphicsTabClicked();
	UFUNCTION() void HandleBackClicked();
};