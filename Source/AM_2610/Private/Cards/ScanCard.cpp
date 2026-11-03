// Fill out your copyright notice in the Description page of Project Settings.

#include "Cards/ScanCard.h"
#include "Engine/World.h"

void UScanCard::Use_Implementation(AActor* User)
{
	UWorld* World = GetWorld();
	if (!World || !PulseClass || !IsValid(User))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = User;
	World->SpawnActor<AScanPulse>(PulseClass, User->GetActorLocation(), FRotator::ZeroRotator, Params);
}