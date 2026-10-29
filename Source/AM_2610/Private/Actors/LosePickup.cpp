// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/LosePickup.h"
#include "Actors/BaseGameMode.h"





void ALosePickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (ABaseGameMode* GM = GetWorld()->GetAuthGameMode<ABaseGameMode>())
	{
		GM->TriggerLose();
	}
}