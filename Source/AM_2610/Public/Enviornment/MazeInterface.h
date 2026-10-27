// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MazeInterface.generated.h"

class AMazeGenerator;

// This class does not need to be modified.
UINTERFACE(BlueprintType, Blueprintable)
class AM_2610_API UMazeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class AM_2610_API IMazeInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.

public:
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Maze|Room")
    void InitializeMazeRoom(AMazeGenerator* Generator);

    // Locking must close the door and immediately block the doorway.
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Maze|Room")
    void SetMazeDoorLocked(FName DoorTag, bool bLocked);

    // Let each enemy react when the player enters or leaves a room.
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Maze|Room")
    void PlayerRoomStateChanged(AActor* Player, bool bIsInRoom);
};
