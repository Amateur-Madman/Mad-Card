// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PickupQueries.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPickupQueries : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class AM_2610_API IPickupQueries
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	//----------------------------------------------------------------------//
	// IPickupQueries
	//----------------------------------------------------------------------//
	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	bool CanPickup(AActor* Target);
	virtual bool CanPickup_Implementation(AActor* Target);
	//----------------------------------------------------------------------//
	// IPickupQueries
	//----------------------------------------------------------------------//
};
