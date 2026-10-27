// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenu.generated.h"

UCLASS()
class AM_2610_API UPauseMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	// Unpauses, restores game input, and removes this widget
	void Resume();

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* ResumeButton;

	// Optional so the widget still compiles/works before the settings menu exists
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UButtonWithText* SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* MainMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* QuitButton;


	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TSubclassOf<class USettingsMenu> SettingsMenuClass;

	UPROPERTY(Transient)
	class USettingsMenu* SettingsMenuInstance = nullptr;
private:
	UFUNCTION() void HandleResumeClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleMainMenuClicked();
	UFUNCTION() void HandleQuitClicked();
	UFUNCTION() void HandleSettingsClosed();
};