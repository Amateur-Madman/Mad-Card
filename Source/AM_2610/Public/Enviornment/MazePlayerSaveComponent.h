// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "MazePlayerSaveComponent.generated.h"

class ABasePlayer;

// Add this to the existing player Blueprint. BasePlayer.h and BasePlayer.cpp stay unchanged.
UCLASS(ClassGroup = (Maze), meta = (BlueprintSpawnableComponent))
class AM_2610_API UMazePlayerSaveComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMazePlayerSaveComponent();

    // Needed when playing the gameplay map directly instead of starting from the menu.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Save")
    FName MainMenuLevel = NAME_None;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<ABasePlayer> Player;
    FTimerHandle SetupTimer;
    int32 SetupAttempts = 0;

    void TryRegisterPlayer();

    UFUNCTION()
    void HandlePlayerLost();
};
