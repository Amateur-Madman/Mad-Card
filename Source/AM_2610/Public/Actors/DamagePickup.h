// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BasePickup.h"
#include "DamagePickup.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ADamagePickup : public ABasePickup
{
	GENERATED_BODY()
	
public:
	ADamagePickup();

protected:
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;

	// Trap persists after triggering - does NOT destroy the actor
	virtual void PostPickup() override;

	// Fire effect - visual only, template/materials assigned in Class Defaults
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	class UParticleSystemComponent* ParticleSystem;

	// Damage applied to whatever overlaps this trap
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DamageAmount = 2.0f;
};
