// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cards/BaseCard.h"
#include "Actors/ScanPulse.h"
#include "ScanCard.generated.h"

UCLASS()
class AM_2610_API UScanCard : public UBaseCard
{
	GENERATED_BODY()

protected:
	virtual void Use_Implementation(AActor* User) override;

	UPROPERTY(EditDefaultsOnly, Category = "Scan")
	TSubclassOf<AScanPulse> PulseClass;
};