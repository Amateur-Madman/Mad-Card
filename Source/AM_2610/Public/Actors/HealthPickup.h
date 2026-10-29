// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/DamagePickup.h"
#include "HealthPickup.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AHealthPickup : public ADamagePickup
{
	GENERATED_BODY()

public:
	AHealthPickup();

protected:
	// Destroys on pickup - bypasses ADamagePickup's persistent-trap override
	// by calling the grandparent's implementation directly
	virtual void PostPickup() override;
	
protected:
	virtual bool CanPickup_Implementation(AActor* Target) override;
};
