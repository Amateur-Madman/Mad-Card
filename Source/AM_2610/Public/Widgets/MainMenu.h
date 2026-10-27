// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenu.generated.h"

class USoundBase;
class UAudioComponent;
/**
 *
 */
UCLASS()
class AM_2610_API UMainMenu : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* PlayButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* QuitButton;

	// Play opens the map that displays WBP_DifficultyMenu.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameFlow|Difficulty")
	FName DifficultyMenuLevel = TEXT("DifficultyMenuMap");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	USoundBase* MainMenuMusic = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MainMenuMusicVolume = 0.4f;

	// Cached once in NativeConstruct - mirrors how WeaponReference/AnimationReference are
	// cached once in ABaseCharacter::BeginPlay, rather than re-cast on every use
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	class UBaseGameInstance* CurrentGameInstance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UButtonWithText* SettingsButton;
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TSubclassOf<class USettingsMenu> SettingsMenuClass;
	UPROPERTY(Transient)
	class USettingsMenu* SettingsMenuInstance = nullptr;

private:
	UPROPERTY(Transient)
	UAudioComponent* MenuMusicComponent = nullptr;

	void StartMenuMusic();
	void StopMenuMusic();

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleSettingsClosed();
};
