// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cards/BaseCard.h"
#include "DecoyCard.generated.h"

class ADecoy;

/**
 * DECOY: places a fake "player" where the crosshair is pointing. While it exists it sits on top of
 * the AI target stack, so enemies chase it instead of the real player.
 */
UCLASS(Blueprintable)
class AM_2610_API UDecoyCard : public UBaseCard
{
	GENERATED_BODY()

protected:
	virtual bool CanUse_Implementation(AActor* User) const override;
	virtual void Use_Implementation(AActor* User) override;

	// Set in the Blueprint child (BP_DecoyCard) to a BP_Decoy
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Decoy")
	TSubclassOf<ADecoy> DecoyClass;

	// Furthest the decoy can be placed from the camera (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Decoy")
	float MaxPlaceDistance = 1500.f;
};
