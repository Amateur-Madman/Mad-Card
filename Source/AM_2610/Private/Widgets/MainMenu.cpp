// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/MainMenu.h"
#include "Both/ButtonWithText.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Utility/BaseGameInstance.h"
#include "Widgets/SettingsMenu.h"
#include "../AM_2610.h"

void UMainMenu::NativeConstruct()
{
	Super::NativeConstruct();
	CurrentGameInstance = Cast<UBaseGameInstance>(GetGameInstance());
	if (!CurrentGameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMainMenu::NativeConstruct - GameInstance is not a UBaseGameInstance"));
	}
	if (PlayButton)
	{
		PlayButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMainMenu::HandlePlayClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMainMenu::HandleQuitClicked);
	}

	// Connect the optional settings button to the existing settings menu.
	if (SettingsButton)
	{
		SettingsButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMainMenu::HandleSettingsClicked);
	}

	StartMenuMusic();
}

void UMainMenu::NativeDestruct()
{
	StopMenuMusic();

	// Remove this menu's bindings when it leaves the viewport.
	if (PlayButton)
	{
		PlayButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMainMenu::HandlePlayClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMainMenu::HandleQuitClicked);
	}

	if (SettingsButton)
	{
		SettingsButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMainMenu::HandleSettingsClicked);
	}

	if (IsValid(SettingsMenuInstance))
	{
		SettingsMenuInstance->OnClosed.RemoveDynamic(this, &UMainMenu::HandleSettingsClosed);
		SettingsMenuInstance->RemoveFromParent();
		SettingsMenuInstance = nullptr;
	}

	CurrentGameInstance = nullptr;
	Super::NativeDestruct();
}

void UMainMenu::StartMenuMusic()
{
	UWorld* World = GetWorld();

	if (!World || !MainMenuMusic || IsValid(MenuMusicComponent))
	{
		return;
	}

	MenuMusicComponent = NewObject<UAudioComponent>(this);

	// Start playback explicitly after registration.
	MenuMusicComponent->SetAutoActivate(false);
	MenuMusicComponent->bAutoDestroy = false;
	MenuMusicComponent->SetUISound(true);

	// Menu music plays at a constant volume, independent of the listener.
	MenuMusicComponent->bAllowSpatialization = false;
	MenuMusicComponent->bOverrideAttenuation = true;
	MenuMusicComponent->AttenuationOverrides.bAttenuate = false;
	MenuMusicComponent->AttenuationOverrides.bSpatialize = false;

	MenuMusicComponent->SetSound(MainMenuMusic);
	MenuMusicComponent->SetVolumeMultiplier(MainMenuMusicVolume);
	MenuMusicComponent->RegisterComponentWithWorld(World);
	MenuMusicComponent->Play();
}

void UMainMenu::StopMenuMusic()
{
	if (IsValid(MenuMusicComponent))
	{
		MenuMusicComponent->Stop();
		MenuMusicComponent->DestroyComponent();
	}

	MenuMusicComponent = nullptr;
}

void UMainMenu::HandlePlayClicked()
{
	if (!GetWorld() || DifficultyMenuLevel.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("Menu: Assign the Difficulty Menu Level map before pressing Play."));
		return;
	}

	// Open difficulty selection before starting a new run.
	StopMenuMusic();
	RemoveFromParent();
	UGameplayStatics::OpenLevel(this, DifficultyMenuLevel);
}

void UMainMenu::HandleQuitClicked()
{
	UBaseGameInstance* GI = Cast<UBaseGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMainMenu::HandleQuitClicked - GameInstance is not a UBaseGameInstance"));
		return;
	}

	StopMenuMusic();
	GI->QuitTheGame();
}

void UMainMenu::HandleSettingsClicked()
{
	if (!SettingsMenuClass) return;

	SettingsMenuInstance = CreateWidget<USettingsMenu>(GetOwningPlayer(), SettingsMenuClass);
	if (!SettingsMenuInstance) return;

	SettingsMenuInstance->OnClosed.AddDynamic(this, &UMainMenu::HandleSettingsClosed);
	SettingsMenuInstance->AddToViewport(10);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UMainMenu::HandleSettingsClosed()
{
	if (IsValid(SettingsMenuInstance))
	{
		SettingsMenuInstance->OnClosed.RemoveDynamic(this, &UMainMenu::HandleSettingsClosed);
	}
	SettingsMenuInstance = nullptr;
	SetVisibility(ESlateVisibility::Visible);
}
