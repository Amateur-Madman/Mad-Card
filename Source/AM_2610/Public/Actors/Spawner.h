// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spawner.generated.h"

UCLASS()
class AM_2610_API ASpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Visual only - assign SM_Barrel_03_Closed (Content/END_Starter/Props/RustyBarrel/)
		// on the Blueprint's mesh component, not hardcoded here
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	class UStaticMeshComponent* SpawnerMesh;

	// Where spawned agents appear - reposition in the Blueprint viewport as needed
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	class USceneComponent* SpawnPoint;

	// Reused unmodified from ABaseCharacter - only needs OnTakeAnyDamage, which every
	// AActor has, so nothing about it is character-specific
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	class UHealthComponent* HealthComponent;

	// Class to spawn - assign BP_Agent in the Blueprint's Class Defaults
	UPROPERTY(EditDefaultsOnly, Category = "Spawner")
	TSubclassOf<class AAgent> AgentClass;

	// Seconds between spawns - also the delay before the first spawn
	UPROPERTY(EditDefaultsOnly, Category = "Spawner")
	float SpawnInterval = 10.0f;

	// Repeating spawns stop once this many agents have been spawned - 0 means unlimited
	// (stops only when the spawner itself is destroyed)
	UPROPERTY(EditDefaultsOnly, Category = "Spawner")
	int32 MaxAgentsToSpawn = 0;

	// Team number given to every agent this spawner creates
	UPROPERTY(EditAnywhere, Category = "Spawner")
	uint8 SpawnedAgentTeam = 2;

	UFUNCTION()
	void HandleDeath(float Percent);

private:
	UFUNCTION()
	void SpawnAgent();

	FTimerHandle SpawnTimerHandle;
	int32 AgentsSpawnedSoFar = 0;
};

