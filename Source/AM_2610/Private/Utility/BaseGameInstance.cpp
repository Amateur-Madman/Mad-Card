// Fill out your copyright notice in the Description page of Project Settings.

#include "Utility/BaseGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Utility/SettingsSaveGame.h"
#include "../AM_2610.h"


void UBaseGameInstance::SetMasterVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.f, 1.f); ApplyAudioSettings(GetWorld()); }
void UBaseGameInstance::SetMusicVolume(float Value) { MusicVolume = FMath::Clamp(Value, 0.f, 1.f); ApplyAudioSettings(GetWorld()); }
void UBaseGameInstance::SetSFXVolume(float Value) { SFXVolume = FMath::Clamp(Value, 0.f, 1.f); ApplyAudioSettings(GetWorld()); }
void UBaseGameInstance::SetDialogueVolume(float Value) { DialogueVolume = FMath::Clamp(Value, 0.f, 1.f); ApplyAudioSettings(GetWorld()); }

void UBaseGameInstance::LoadCurrentLevel()
{
	LoadLevelSafe(CurrentLevel);
}

void UBaseGameInstance::QuitToMenu()
{
	LoadLevelSafe(0);
}
void UBaseGameInstance::LoadFirstLevel()
{
	LoadLevelSafe(FirstLevelIndex);
}

void UBaseGameInstance::QuitTheGame()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}

void UBaseGameInstance::LoadLevelSafe(int32 LevelIndex)
{
	if (!GameLevels.IsValidIndex(LevelIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadLevelSafe - Level index [%d] is invalid in the game instance"), LevelIndex);
		return;
	}

	CurrentLevel = LevelIndex;
	UGameplayStatics::OpenLevel(this, GameLevels[CurrentLevel]);
}
static const FString SettingsSlot = TEXT("Settings");

void UBaseGameInstance::Init()
{
	Super::Init();
	LoadSettings();
}

void UBaseGameInstance::OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
	Super::OnWorldChanged(OldWorld, NewWorld);

	if (NewWorld)
	{
		const FName LevelName(*UGameplayStatics::GetCurrentLevelName(NewWorld, true));
		const int32 LevelIndex = GameLevels.IndexOfByKey(LevelName);
		if (LevelIndex != INDEX_NONE) CurrentLevel = LevelIndex;

		// Audio device isn't ready yet at this point, so apply on the next tick
		TWeakObjectPtr<UWorld> WeakWorld = NewWorld;
		NewWorld->GetTimerManager().SetTimerForNextTick([this, WeakWorld]()
			{
				if (WeakWorld.IsValid()) ApplyAudioSettings(WeakWorld.Get());
			});
	}
}

void UBaseGameInstance::ApplyAudioSettings(UWorld* World)
{
	if (!World || !SettingsSoundMix) return;

	// Activate the mix once per world (pushing repeatedly stacks ref counts)
	if (MixPushedWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, SettingsSoundMix);
		MixPushedWorld = World;
	}

	auto Apply = [&](USoundClass* Class, float Volume, bool bChildren)
		{
			if (Class) UGameplayStatics::SetSoundMixClassOverride(World, SettingsSoundMix, Class, Volume, 1.f, 0.f, bChildren);
		};
	Apply(MasterSoundClass, MasterVolume, true);   // master cascades to children
	Apply(MusicSoundClass, MusicVolume, false);
	Apply(SFXSoundClass, SFXVolume, false);
	Apply(DialogueSoundClass, DialogueVolume, false);
}
void UBaseGameInstance::SaveSettings()
{
	UE_LOG(LogTemp, Warning, TEXT("SaveSettings %.2f/%.2f/%.2f"), MasterVolume, MusicVolume, SFXVolume);
	USettingsSaveGame* Save = Cast<USettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass()));
	if (!Save) return;
	Save->MasterVolume = MasterVolume;
	Save->MusicVolume = MusicVolume;
	Save->SFXVolume = SFXVolume;
	Save->DialogueVolume = DialogueVolume;
	UGameplayStatics::SaveGameToSlot(Save, SettingsSlot, 0);
}

void UBaseGameInstance::LoadSettings()
{
	if (!UGameplayStatics::DoesSaveGameExist(SettingsSlot, 0)) return;
	if (const USettingsSaveGame* Save = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0)))
	{
		MasterVolume = Save->MasterVolume;
		MusicVolume = Save->MusicVolume;
		SFXVolume = Save->SFXVolume;
	}
}