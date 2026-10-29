// Fill out your copyright notice in the Description page of Project Settings.


#include "Utility/PickupQueries.h"

// Add default functionality here for any IPickupQueries functions that are not pure virtual.

bool IPickupQueries::CanPickup_Implementation(AActor* Target)
{
	return true;
}