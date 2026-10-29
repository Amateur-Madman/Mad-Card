// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BasePlayerController.h"
#include "Actors/BaseGameMode.h"
#include "Widgets/PauseMenu.h"
#include "Kismet/GameplayStatics.h"

void ABasePlayerController::TogglePause()
{
	// Already open: close it
	if (IsValid(PauseMenuInstance) && PauseMenuInstance->IsInViewport())
	{
		PauseMenuInstance->Resume();
		return;
	}

	// Don't pause over the win/lose screen
	if (const ABaseGameMode* GM = GetWorld()->GetAuthGameMode<ABaseGameMode>())
	{
		if (GM->IsGameOver()) return;
	}

	if (!PauseMenuClass) return;

	PauseMenuInstance = CreateWidget<UPauseMenu>(this, PauseMenuClass);
	if (!PauseMenuInstance) return;

	PauseMenuInstance->AddToViewport();
	UGameplayStatics::SetGamePaused(this, true);

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(PauseMenuInstance->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(Mode);
	FlushPressedKeys();
	SetShowMouseCursor(true);
}
void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(true); // matches the BP node's "Flush Input" checkbox
	SetInputMode(InputMode);
}
