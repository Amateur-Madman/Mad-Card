// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WinPickup.h"
#include "Particles/ParticleSystemComponent.h"
#include "Actors/BaseGameMode.h"
#include "Components/BoxComponent.h"


AWinPickup::AWinPickup()
{
	ParticleSystem = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleSystem"));
	ParticleSystem->SetupAttachment(BoxCollision);
}

bool AWinPickup::CanPickup_Implementation(AActor* Other)
{
	return IsPlayerPawn(Other);
}

void AWinPickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (ABaseGameMode* GM = GetWorld()->GetAuthGameMode<ABaseGameMode>())
	{
		GM->TriggerWin();
	}
}
