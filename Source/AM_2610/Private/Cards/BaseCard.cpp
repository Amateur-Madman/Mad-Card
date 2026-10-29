// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/BaseCard.h"
#include "Engine/World.h"

bool UBaseCard::TryUse(AActor* User)
{
	if (!IsValid(User) || !CanUse(User))
	{
		return false;
	}

	Use(User);
	return true;
}

UWorld* UBaseCard::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* OuterObject = GetOuter();
	return OuterObject ? OuterObject->GetWorld() : nullptr;
}

bool UBaseCard::CanUse_Implementation(AActor* User) const
{
	return true;
}

void UBaseCard::Use_Implementation(AActor* User)
{
	// Intentionally empty - implemented per card
}
