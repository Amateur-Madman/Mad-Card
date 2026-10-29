// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/WinPickup.h"
#include "LosePickup.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ALosePickup : public AWinPickup
{
	GENERATED_BODY()
	
protected:
	
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;
};
