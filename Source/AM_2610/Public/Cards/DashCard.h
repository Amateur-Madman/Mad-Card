// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Cards/BaseCard.h"
#include "DashCard.generated.h"

/**
 * DASH: a burst of speed in the direction the player is moving, and for a short window
 * the player ignores the channels in PhaseChannels, so they slip through enemies and
 * enemy overlap spheres (no catch / grab / kill triggers mid-dash).
 */
UCLASS(Blueprintable)
class AM_2610_API UDashCard : public UBaseCard
{
	GENERATED_BODY()

protected:
	virtual bool CanUse_Implementation(AActor* User) const override;
	virtual void Use_Implementation(AActor* User) override;

	// Horizontal launch speed (cm/s). Ground friction brakes quickly, so tune the
	// distance by feel - raise this or lower the character's braking friction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float DashSpeed = 2400.f;

	// How long (seconds) the player is intangible to enemies
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float PhaseDuration = 0.45f;

	// Channels the player ignores while dashing. Must include the OBJECT TYPE of every enemy's
	// ProximitySphere (check it in the enemy Blueprint) plus Pawn for body-to-body passing.
	// Keep WorldStatic out of this list so walls still block.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	TArray<TEnumAsByte<ECollisionChannel>> PhaseChannels{ ECC_Pawn, ECC_WorldDynamic };
};