// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Agent.h"
#include "MadMan_Agent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLocation);

/**
 * 
 */
UCLASS()
class AM_2610_API AMadMan_Agent : public AAgent
{
	GENERATED_BODY()
	
public:
	AMadMan_Agent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	AActor* IntroSpline;

	FOnLocation OnIntroSpot;

	void SetFoucsForRotation(AActor* Target);

protected:
	virtual void BeginPlay() override;

	class AWrittingBoard* WrittingBoard;

private:
	FRotator TargetInPlaceRotation;
	float InPlaceTurnSpeed = 2.0f;

	UFUNCTION()
	void IntroduceGame();
};
