// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Agent.h"
#include "Enviornment/MazeInterface.h"
#include "Chaser_Agent.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AChaser_Agent : public AAgent
{
	GENERATED_BODY()
public:
	//AChaser_Agent();

	//----------------------------------------------------------------------//
	// MazeInterface
	//----------------------------------------------------------------------//
	//virtual void PlayerRoomStateChanged_Implementation(AActor* Player, bool bIsInRoom) override;
protected:
	void BeginPlay() override;

private:
	UFUNCTION()
	void OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
