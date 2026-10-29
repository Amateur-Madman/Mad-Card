// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/PauseMenu.h"
#include "Both/ButtonWithText.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SettingsMenu.h"
#include "Utility/BaseGameInstance.h"
#include "Actors/BasePlayer.h"

void UPauseMenu::NativeConstruct()
{
	Super::NativeConstruct();


	if (ABasePlayer* Player = Cast<ABasePlayer>(GetOwningPlayerPawn()))
	{
		Player->SetHUDHidden(true);
	}
	if (ResumeButton)   ResumeButton->OnBackgroundButtonClicked.AddDynamic(this, &UPauseMenu::HandleResumeClicked);
	if (SettingsButton) SettingsButton->OnBackgroundButtonClicked.AddDynamic(this, &UPauseMenu::HandleSettingsClicked);
	if (MainMenuButton) MainMenuButton->OnBackgroundButtonClicked.AddDynamic(this, &UPauseMenu::HandleMainMenuClicked);
	if (QuitButton)     QuitButton->OnBackgroundButtonClicked.AddDynamic(this, &UPauseMenu::HandleQuitClicked);
}

void UPauseMenu::NativeDestruct()
{
	if (ABasePlayer* Player = Cast<ABasePlayer>(GetOwningPlayerPawn()))
	{
		Player->SetHUDHidden(false);
	}
	if (IsValid(SettingsMenuInstance))
	{
		SettingsMenuInstance->OnClosed.RemoveDynamic(this, &UPauseMenu::HandleSettingsClosed);
		SettingsMenuInstance->RemoveFromParent();
		SettingsMenuInstance = nullptr;
	}
	Super::NativeDestruct();
}

void UPauseMenu::Resume()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(true);
		PC->SetInputMode(Mode);
		PC->FlushPressedKeys();
		PC->SetShowMouseCursor(false);
	}
	UGameplayStatics::SetGamePaused(this, false);
	RemoveFromParent();
}

void UPauseMenu::HandleResumeClicked() { Resume(); }

void UPauseMenu::HandleSettingsClicked()
{
	if (IsValid(SettingsMenuInstance)) return;
	if (!SettingsMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPauseMenu: SettingsMenuClass not set"));
		return;
	}

	SettingsMenuInstance = CreateWidget<USettingsMenu>(GetOwningPlayer(), SettingsMenuClass);
	if (!SettingsMenuInstance) return;

	SettingsMenuInstance->OnClosed.AddDynamic(this, &UPauseMenu::HandleSettingsClosed);
	SettingsMenuInstance->AddToViewport(10);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPauseMenu::HandleSettingsClosed()
{
	if (IsValid(SettingsMenuInstance))
	{
		SettingsMenuInstance->OnClosed.RemoveDynamic(this, &UPauseMenu::HandleSettingsClosed);
	}
	SettingsMenuInstance = nullptr;
	SetVisibility(ESlateVisibility::Visible);
}

void UPauseMenu::HandleMainMenuClicked()
{
	UGameplayStatics::SetGamePaused(this, false); // unpause before leaving the level
	if (UBaseGameInstance* GI = Cast<UBaseGameInstance>(GetGameInstance()))
	{
		GI->QuitToMenu();
	}
}

void UPauseMenu::HandleQuitClicked()
{
	if (UBaseGameInstance* GI = Cast<UBaseGameInstance>(GetGameInstance()))
	{
		GI->QuitTheGame();
	}
}
