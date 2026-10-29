// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BaseGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UBaseGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	// Loads GameLevels[FirstLevelIndex] via LoadLevelSafe - bound to the main menu's Play button
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void LoadFirstLevel();

	// Quits the application - bound to the main menu's Quit button
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void QuitTheGame();

	// Reloads GameLevels[CurrentLevel] via LoadLevelSafe - bound to the results widget's Play Again button
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void LoadCurrentLevel();

	// Loads GameLevels[0] (the main menu) via LoadLevelSafe - bound to the results widget's Return To Main Menu button
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void QuitToMenu();

	virtual void Init() override;
	virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) override;

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio") void SetMasterVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio") void SetMusicVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio") void SetSFXVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio") void SetDialogueVolume(float Value);
	UFUNCTION(BlueprintPure, Category = "Settings|Audio") float GetMasterVolume() const { return MasterVolume; }
	UFUNCTION(BlueprintPure, Category = "Settings|Audio") float GetMusicVolume() const { return MusicVolume; }
	UFUNCTION(BlueprintPure, Category = "Settings|Audio") float GetSFXVolume() const { return SFXVolume; }
	UFUNCTION(BlueprintPure, Category = "Settings|Audio") float GetDialogueVolume() const { return DialogueVolume; }
	void SaveSettings();
protected:
	// Opens GameLevels[LevelIndex] and updates CurrentLevel if the index is valid;
	// logs a warning and does nothing otherwise
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void LoadLevelSafe(int32 LevelIndex);

	// Level names this game can load, indexed the same way FirstLevelIndex/CurrentLevel reference them
	UPROPERTY(EditDefaultsOnly, Category = "GameFlow")
	TArray<FName> GameLevels;

	// Index into GameLevels that LoadFirstLevel() loads
	UPROPERTY(EditDefaultsOnly, Category = "GameFlow")
	int32 FirstLevelIndex = 0;

	// Index into GameLevels most recently loaded via LoadLevelSafe
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	int32 CurrentLevel = 0;

	void LoadSettings();
	void ApplyAudioSettings(UWorld* World);

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio") class USoundMix* SettingsSoundMix;
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio") class USoundClass* MasterSoundClass;
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio") class USoundClass* MusicSoundClass;
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio") class USoundClass* SFXSoundClass;
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Audio") class USoundClass* DialogueSoundClass;

	TWeakObjectPtr<UWorld> MixPushedWorld;
	float MasterVolume = 1.f;
	float MusicVolume = 1.f;
	float SFXVolume = 1.f;
	float DialogueVolume = 1.f;
	
};
