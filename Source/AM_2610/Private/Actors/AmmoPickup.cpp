// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/AmmoPickup.h"
#include "Actors/BasePlayer.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AAmmoPickup::AAmmoPickup()
{
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(BoxCollision);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAmmoPickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (ABasePlayer* Player = Cast<ABasePlayer>(OtherActor))
	{
		//increase ammo
		Player->AddMaxAmmo(AmmoIncreaseAmount);
	}
}

bool AAmmoPickup::CanPickup_Implementation(AActor* Target)
{
	return IsPlayerPawn(Target);
}