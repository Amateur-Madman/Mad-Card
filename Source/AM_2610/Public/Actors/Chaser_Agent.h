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
class AM_2610_API AChaser_Agent : public AAgent, public IMazeInterface
{
	GENERATED_BODY()
public:
	AChaser_Agent();

	//----------------------------------------------------------------------//
	// MazeInterface
	//----------------------------------------------------------------------//
	virtual void PlayerRoomStateChanged_Implementation(AActor* Player, bool bIsInRoom) override;
protected:
	void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Default)
	float SpeedIncrement = 25.0f;

private:
	UFUNCTION()
	void OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	float InitialSpeed = 0.0f;

	bool Roared = false;

	void RoarAtSpeedThreshold();

	UFUNCTION()
	void TrackStar();

	void PlayRoarSound();
};
