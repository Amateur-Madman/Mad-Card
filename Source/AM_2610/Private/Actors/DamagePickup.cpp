// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/DamagePickup.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"

ADamagePickup::ADamagePickup()
{
	ParticleSystem = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleSystem"));
	ParticleSystem->SetupAttachment(BoxCollision);
}

void ADamagePickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		UGameplayStatics::ApplyDamage(OtherActor, DamageAmount, nullptr, this, nullptr);
	}
}

void ADamagePickup::PostPickup()
{
	// Deliberately does NOT call Super::PostPickup_Implementation() / Destroy() -
	// this pickup is a persistent trap, not a one-time consumable.
}
