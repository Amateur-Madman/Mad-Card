// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MazePuzzleComponent.generated.h"

class AMazeGenerator;
class AMazePuzzleButton;
class APawn;
class UArrowComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPuzzleButtonsUnlocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleConsoleStateChanged, bool, bRed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPuzzleConsoleActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomPuzzleCompleted);

// Add this component to a room without changing its BP_RoomBase parent.
UCLASS(ClassGroup = (Maze), meta = (BlueprintSpawnableComponent))
class AM_2610_API UMazePuzzleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMazePuzzleComponent();

    // Disable this for the other puzzles if they use different entry conditions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    bool bUsePressureButtons = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    TSubclassOf<AMazePuzzleButton> ButtonClass;

    // Two different tagged arrows are selected for the two buttons.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    FName ButtonSpawnTag = TEXT("PuzzleButtonSpawn");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    FName PuzzleDoorTag = TEXT("PuzzleDoor");

    // The enemy class comes from the generator's Weeping Angel type tag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Enemies", meta = (ClampMin = "0", ClampMax = "16"))
    int32 WeepingAngelCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Enemies")
    FName EnemySpawnTag = TEXT("PuzzleEnemySpawn");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    FName PuzzleId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleStarted = false;

    // Both buttons make the console ready. They do not unlock the door directly.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bButtonsUnlocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleDoorUnlocked = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bConsoleRed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    TArray<TObjectPtr<AMazePuzzleButton>> SpawnedButtons;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    TArray<TObjectPtr<APawn>> SpawnedWeepingAngels;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleButtonsUnlocked OnButtonsUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleConsoleStateChanged OnConsoleStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleConsoleActivated OnConsoleActivated;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnRoomPuzzleCompleted OnPuzzleCompleted;

    // Call after the player has fully entered the pressure-button room.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool BeginPuzzle(APawn* Player);

    // Call from the console's existing player interaction event.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool ActivateConsole(APawn* Player);

    // The actual puzzle's success logic unlocks the separate maze end door.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CompletePuzzle();

    bool PreparePuzzle(AMazeGenerator* Generator, FName SelectedId, TSubclassOf<APawn> AngelClass, int32 Seed, bool bSpawnAllButtons = false, bool bSpawnAllEnemies = false);
    bool StepButtonSpawning(bool& bFinished);
    bool StepEnemySpawning(bool& bFinished);
    void ArmPuzzle();

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<AMazeGenerator> MazeGenerator;

    UPROPERTY(Transient)
    TSubclassOf<APawn> WeepingAngelClass;

    // Extra debug buttons are previews. Only SpawnedButtons are used by the puzzle.
    UPROPERTY(Transient)
    TArray<TObjectPtr<AMazePuzzleButton>> DebugButtons;

    TArray<TWeakObjectPtr<UArrowComponent>> ButtonSpawns;
    TArray<TWeakObjectPtr<UArrowComponent>> EnemySpawns;
    int32 ButtonSpawnIndex = 0;
    int32 EnemySpawnIndex = 0;
    int32 RequestedEnemyCount = 0;
    int32 RequestedButtonCount = 2;
    bool bDebugSpawnAllButtons = false;
    bool bDebugSpawnAllEnemies = false;
    bool bPrepared = false;
    bool bArmed = false;

    bool CanUnlockButtons(const TArray<APawn*>& First, const TArray<APawn*>& Second) const;
    bool IsEligibleOccupant(APawn* Pawn) const;
    void ClearSpawnedActors();
};
