// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePlayerSaveComponent.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Actors/BasePlayer.h"
#include "Engine/World.h"

UMazePlayerSaveComponent::UMazePlayerSaveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMazePlayerSaveComponent::BeginPlay()
{
    Super::BeginPlay();
    Player = Cast<ABasePlayer>(GetOwner());

    if (!Player.IsValid() || !GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("Maze save: MazePlayerSaveComponent needs an actor derived from BasePlayer."));
        return;
    }

    Player->OnPlayerLost.AddUniqueDynamic(this, &UMazePlayerSaveComponent::HandlePlayerLost);

    // The room, player possession, and the player's own BeginPlay may finish in either order.
    GetWorld()->GetTimerManager().SetTimer(SetupTimer, this, &UMazePlayerSaveComponent::TryRegisterPlayer, 0.05f, true);
}

void UMazePlayerSaveComponent::TryRegisterPlayer()
{
    UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);

    if (IsValid(SaveSystem) && SaveSystem->RegisterPlayer(Player.Get(), MainMenuLevel))
    {
        GetWorld()->GetTimerManager().ClearTimer(SetupTimer);
        return;
    }

    if (++SetupAttempts >= 200)
    {
        GetWorld()->GetTimerManager().ClearTimer(SetupTimer);

        if (IsValid(SaveSystem))
        {
            SaveSystem->ReportRestoreTimeout();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Maze save: The game instance has no MazeSaveSubsystem."));
        }
    }
}

void UMazePlayerSaveComponent::HandlePlayerLost()
{
    UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);

    if (!IsValid(SaveSystem) || !SaveSystem->HandlePlayerDeath(Player.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze save: Death was not accepted. Check player registration and the start-room checkpoint."));
    }
}

void UMazePlayerSaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SetupTimer);
    }

    if (Player.IsValid())
    {
        Player->OnPlayerLost.RemoveDynamic(this, &UMazePlayerSaveComponent::HandlePlayerLost);
    }

    Super::EndPlay(EndPlayReason);
}
