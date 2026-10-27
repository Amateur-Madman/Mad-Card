// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "Actors/BasePickup.h"
#include "WinPickup.generated.h"

UCLASS()
class AM_2610_API AWinPickup : public ABasePickup
{
	GENERATED_BODY()
public:
	AWinPickup();
protected:
	virtual bool CanPickup_Implementation(AActor* Target) override;
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UParticleSystemComponent* ParticleSystem;
};

