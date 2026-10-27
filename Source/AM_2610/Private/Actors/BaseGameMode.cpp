// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BaseGameMode.h"
#include "Actors/BaseCharacter.h"
#include "Actors/BasePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/PauseMenu.h"
#include "Widgets/ResultsWidget.h"
#include "Enviornment/MazeSaveSubsystem.h"

void ABaseGameMode::BeginPlay()
{
	Super::BeginPlay();



	if (CurrentPlayer)
	{
		CurrentPlayer->OnPlayerLost.AddDynamic(this, &ABaseGameMode::RemovePlayer);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGameMode::BeginPlay - no BasePlayer found in the level, cannot bind OnPlayerLost"));
	}

	UClass* ResolvedResultsWidgetClass = ResultsWidgetClass.LoadSynchronous();
	if (!ResolvedResultsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGameMode::BeginPlay - failed to load ResultsWidgetClass from soft reference"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetActorOfClass(this, PlayerControllerClass));
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGameMode::BeginPlay - no actor matching PlayerControllerClass found, cannot create ResultsWidgetObject"));
		return;
	}

	ResultsWidgetObject = CreateWidget<UResultsWidget>(PC, ResolvedResultsWidgetClass);
	if (!ResultsWidgetObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGameMode::BeginPlay - failed to create ResultsWidgetObject"));
	}
}

void ABaseGameMode::RegisterEnemy(AActor* NewEnemy)
{
	if (!NewEnemy)
	{
		return;
	}

	NumberOfEnemies++;
	NewEnemy->OnDestroyed.AddDynamic(this, &ABaseGameMode::RemoveEnemy);
}
void ABaseGameMode::RemoveEnemy(AActor* DestroyedActor)
{
	UE_LOG(LogTemp, Warning, TEXT("RemoveEnemy called - NumberOfEnemies before decrement: %d"), NumberOfEnemies);
	NumberOfEnemies--;

	if (NumberOfEnemies > 0)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ABaseGameMode::RemoveEnemy - Player won"));
	if (ResultsWidgetObject)
	{
		ResultsWidgetObject->AddToViewport();
		ResultsWidgetObject->WinConditionMet();
	}

	if (CurrentPlayer)
	{
		CurrentPlayer->PlayerWon();
	}
}
void ABaseGameMode::TriggerWin()
{
	if (bGameOver) return;
	bGameOver = true;

	ShowResults();
	if (ResultsWidgetObject)
	{
		ResultsWidgetObject->WinConditionMet();
	}
	if (CurrentPlayer)
	{
		CurrentPlayer->PlayerWon();
	}
}

void ABaseGameMode::TriggerLose()
{
	if (bGameOver) return;
	bGameOver = true;

	if (!IsValid(CurrentPlayer))
	{
		CurrentPlayer = Cast<ABasePlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
	}

	// Direct enemy losses also need to record the death before showing Play Again.
	UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
	if (!IsValid(SaveSystem) || !SaveSystem->HandlePlayerDeath(CurrentPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("Maze save: TriggerLose could not record the death. Check player registration and the start-room checkpoint."));
	}

	ShowResults();
	if (CurrentPlayer)
	{
		CurrentPlayer->FreezeForResults();
	}
}
void ABaseGameMode::RemovePlayer()
{
	TriggerLose();
	
}

void ABaseGameMode::ShowResults()
{
	if (!ResultsWidgetObject) return;

	ResultsWidgetObject->AddToViewport();

	if (APlayerController* PC = ResultsWidgetObject->GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ResultsWidgetObject->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PC->SetInputMode(InputMode);
		PC->FlushPressedKeys();
	}
}