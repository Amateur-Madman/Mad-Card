// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BasePickup.h"
#include "AmmoPickup.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AAmmoPickup : public ABasePickup
{
	GENERATED_BODY()
	
public:
	AAmmoPickup();

protected:
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;

	virtual bool CanPickup_Implementation(AActor* Target) override;

	// Visual only - assign SM_MilitaryCrateClosed_03 (Content/END_Starter/Props/MilitaryCrates/)
	// on the Blueprint's mesh component, not hardcoded here - this assignment disallows ConstructorHelpers
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
	class UStaticMeshComponent* PickupMesh;

	// Added to the player's rifle MaxAmmo on pickup
	UPROPERTY(EditDefaultsOnly, Category = "Ammo")
	float AmmoIncreaseAmount = 3.0f;
};
