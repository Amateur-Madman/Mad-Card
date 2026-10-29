// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazeCardPickup.h"
#include "Enviornment/MazeSaveSubsystem.h"

void AMazeCardPickup::BeginPlay()
{
    const UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
    const bool bHard = IsValid(SaveSystem) && SaveSystem->GetDifficulty() == EMazeDifficulty::Hard;
    bSecondChanceSelected = bHard && (Reward == EMazePickupReward::SecondChance || (Reward == EMazePickupReward::Random && FMath::FRand() < FMath::Clamp(SecondChanceChance, 0.0f, 1.0f)));

    if (!bHard && Reward == EMazePickupReward::SecondChance)
    {
        // A fixed charge pickup becomes Dash or Decoy on Easy.
        RandomCardClasses = EasyReplacementCardClasses;
        RandomCardClasses.RemoveAll([](const TSubclassOf<UBaseCard>& Class)
            {
                return !Class;
            });

        if (RandomCardClasses.IsEmpty())
        {
            CardClass = nullptr;
            UE_LOG(LogTemp, Error, TEXT("Maze save: %s needs Dash and Decoy replacement classes for Easy mode."), *GetName());
        }
    }

    // The existing CardPickup rolls once before running its Blueprint BeginPlay.
    Super::BeginPlay();
}

bool AMazeCardPickup::CanPickup_Implementation(AActor* Target)
{
    if (bSecondChanceSelected)
    {
        const UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
        return IsPlayerPawn(Target) && IsValid(SaveSystem) && SaveSystem->CanAddSecondChance();
    }

    return Super::CanPickup_Implementation(Target);
}

void AMazeCardPickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
    if (bSecondChanceSelected)
    {
        UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
        bGrantedSecondChance = IsValid(SaveSystem) && SaveSystem->AddSecondChanceCharge();
        return;
    }

    Super::HandlePickup(OtherActor, SweepResult);
}

void AMazeCardPickup::PostPickup()
{
    if (bSecondChanceSelected)
    {
        // A failed save leaves the pickup available. Successful charges are single use.
        if (bGrantedSecondChance)
        {
            ABasePickup::PostPickup();
        }

        return;
    }

    Super::PostPickup();
}
