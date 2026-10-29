// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MazeActors.generated.h"

class UArrowComponent;

UCLASS()
class AM_2610_API AMazeActors : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMazeActors();

	UFUNCTION(BlueprintPure, Category = "Maze")
	TArray<UArrowComponent*> GetConnections() const;

	// True while the player overlaps a room's RoomArea trigger.
	UFUNCTION(BlueprintPure, Category = "Maze|Rooms")
	static bool IsPlayerInRoom(AActor* Player);
};
