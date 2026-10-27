// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/CardPickup.h"
#include "MazeCardPickup.generated.h"

UENUM(BlueprintType)
enum class EMazePickupReward : uint8
{
    Card,
    SecondChance,
    Random
};

// Reparent the pickup Blueprint to this class to keep its mesh and inherited pickup behavior.
UCLASS()
class AM_2610_API AMazeCardPickup : public ACardPickup
{
    GENERATED_BODY()

public:
    virtual bool CanPickup_Implementation(AActor* Target) override;

protected:
    virtual void BeginPlay() override;
    virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;
    virtual void PostPickup() override;

    // Card keeps the existing Dash/Decoy behavior. SecondChance is a fixed charge pickup.
    // Random rolls for a charge, then uses the inherited card pool for the other results.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|SecondChance")
    EMazePickupReward Reward = EMazePickupReward::Card;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|SecondChance", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Reward == EMazePickupReward::Random"))
    float SecondChanceChance = 0.333333f;

    // Assign Dash and Decoy here for a fixed Second Chance pickup's Easy-mode replacement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|SecondChance")
    TArray<TSubclassOf<UBaseCard>> EasyReplacementCardClasses;

    // Available before Blueprint BeginPlay if the display needs to choose a material.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Pickup|SecondChance")
    bool bSecondChanceSelected = false;

private:
    bool bGrantedSecondChance = false;
};
