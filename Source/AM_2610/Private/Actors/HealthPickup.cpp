// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/HealthPickup.h"

AHealthPickup::AHealthPickup()
{
	DamageAmount = -1.6f;
}

void AHealthPickup::PostPickup()
{

	// ABasePickup's version directly, so this pickup still destroys itself.
	ABasePickup::PostPickup();
}

// HealthPickup.cpp
bool AHealthPickup::CanPickup_Implementation(AActor* Target)
{
	const APawn* Pawn = Cast<APawn>(Target);
	return Pawn && Pawn->IsPlayerControlled();
}

