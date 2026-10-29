// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Agent.h"
#include "Utility/EnemyBehaviorInterface.h"
#include "WeepingAngel_Agent.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AWeepingAngel_Agent : public AAgent, public IEnemyBehaviorInterface
{
	GENERATED_BODY()
public:
	AWeepingAngel_Agent();

	//----------------------------------------------------------------------//
	// MazeInterface
	//----------------------------------------------------------------------//
	//virtual void PlayerRoomStateChanged_Implementation(AActor* Player, bool bIsInRoom) override;

	//----------------------------------------------------------------------//
	// IEnemyBehaviorInterface
	//----------------------------------------------------------------------//
	virtual void PlayerSeesMe_Implementation(bool bIsSeen) override;

protected:
	void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perception")
	class UAIPerceptionStimuliSourceComponent* StimComponent;

private:
	UFUNCTION()
	void OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool CanKillPlayer;
};
