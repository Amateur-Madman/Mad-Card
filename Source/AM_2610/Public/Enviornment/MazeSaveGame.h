// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MazeSaveGame.generated.h"

class AMazeActors;

UENUM(BlueprintType)
enum class EMazeDifficulty : uint8
{
    Easy,
    Hard
};

// Save the room and its progress. A generated maze is deliberately not part of a checkpoint.
USTRUCT(BlueprintType)
struct AM_2610_API FMazeStartRoomSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    TSoftClassPtr<AMazeActors> RoomClass;

    UPROPERTY(SaveGame)
    FTransform RoomTransform = FTransform::Identity;

    UPROPERTY(SaveGame)
    FTransform PlayerTransform = FTransform::Identity;

    UPROPERTY(SaveGame)
    FName EntranceDoorTag = NAME_None;

    UPROPERTY(SaveGame)
    TArray<FName> UnlockedDoorTags;

    UPROPERTY(SaveGame)
    TArray<FName> CompletedPuzzleIds;

    // Door tags retain their puzzle across checkpoints, reloads, and completed-door replays.
    UPROPERTY(SaveGame)
    TMap<FName, FName> DoorPuzzleAssignments;

    UPROPERTY(SaveGame)
    int32 RoomsCompleted = 0;

    UPROPERTY(SaveGame)
    TSoftClassPtr<AMazeActors> LastRouteRoomClass;

    UPROPERTY(SaveGame)
    int32 ConsecutiveRouteRooms = 0;

    UPROPERTY(SaveGame)
    bool bValid = false;
};

// The menu can read this without knowing how the save is stored.
USTRUCT(BlueprintType)
struct AM_2610_API FMazeSaveSummary
{
    GENERATED_BODY()

    // SlotIndex starts at zero. Display SlotIndex + 1 to the player.
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    int32 SlotIndex = INDEX_NONE;

    // An unreadable or ended save still occupies a slot, but cannot be loaded.
    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    bool bSlotOccupied = false;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    bool bHasSave = false;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    EMazeDifficulty Difficulty = EMazeDifficulty::Hard;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    int32 SecondChanceCharges = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    int32 MazesCompleted = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    FDateTime SavedAt;
};

UCLASS()
class AM_2610_API UMazeSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 SaveVersion = 1;

    UPROPERTY(SaveGame)
    FGuid RunId;

    UPROPERTY(SaveGame)
    EMazeDifficulty Difficulty = EMazeDifficulty::Hard;

    UPROPERTY(SaveGame)
    int32 SecondChanceCharges = 0;

    UPROPERTY(SaveGame)
    FName GameplayLevel = NAME_None;

    UPROPERTY(SaveGame)
    FName MainMenuLevel = NAME_None;

    UPROPERTY(SaveGame)
    FDateTime SavedAt;

    UPROPERTY(SaveGame)
    FMazeStartRoomSaveData Checkpoint;
};

// Charges and final deaths update independently of the table's saved checkpoint.
USTRUCT()
struct AM_2610_API FMazeSavedRunState
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    int32 SecondChanceCharges = 0;

    UPROPERTY(SaveGame)
    bool bRunEnded = false;
};

UCLASS()
class AM_2610_API UMazeRunLedger : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 SaveVersion = 1;

    UPROPERTY(SaveGame)
    TMap<FGuid, FMazeSavedRunState> RunStates;
};