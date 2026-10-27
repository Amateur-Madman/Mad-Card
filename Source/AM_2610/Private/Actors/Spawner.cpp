// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/Spawner.h"
#include "Actors/Agent.h"
#include "Actors/BaseGameMode.h"
#include "Utility/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

ASpawner::ASpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpawnerMesh"));
	SetRootComponent(SpawnerMesh);

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(SpawnerMesh);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	
}

void ASpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ASpawner::HandleDeath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASpawner::BeginPlay - HealthComponent is null, cannot bind OnDeath"));
	}


	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ASpawner::SpawnAgent, SpawnInterval, true);
}

void ASpawner::SpawnAgent()
{
	// Stop the timer once the spawn cap is reached (0 = unlimited)
	if (MaxAgentsToSpawn > 0 && AgentsSpawnedSoFar >= MaxAgentsToSpawn)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}

	if (!AgentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASpawner::SpawnAgent - AgentClass is not set"));
		return;
	}

	FTransform SpawnTransform = SpawnPoint ? SpawnPoint->GetComponentTransform() : GetActorTransform();

	// Deferred spawn: the actor is created but NOT yet initialized, so we can set the
	// team before its AI controller possesses it and perception registers.
	AAgent* SpawnedAgent = GetWorld()->SpawnActorDeferred<AAgent>(
		AgentClass,
		SpawnTransform,
		nullptr,   // Owner
		nullptr,   // Instigator
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!SpawnedAgent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASpawner::SpawnAgent - SpawnActorDeferred failed"));
		return;
	}

	// Set the team BEFORE FinishSpawning
	//SpawnedAgent->SetGenericTeamId(FGenericTeamId(SpawnedAgentTeam));

	// Completes construction/initialization; the AI controller possesses the agent here
	SpawnedAgent->FinishSpawning(SpawnTransform);

	AgentsSpawnedSoFar++;

}
void ASpawner::HandleDeath(float Percent)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Destroy();
}