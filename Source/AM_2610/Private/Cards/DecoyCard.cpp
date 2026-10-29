// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/DecoyCard.h"
#include "Actors/Decoy.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

bool UDecoyCard::CanUse_Implementation(AActor* User) const
{
	return DecoyClass && User && User->FindComponentByClass<UCameraComponent>();
}

void UDecoyCard::Use_Implementation(AActor* User)
{
	UWorld* World = User->GetWorld();
	const UCameraComponent* Camera = User->FindComponentByClass<UCameraComponent>();
	if (!World || !Camera)
	{
		return;
	}

	// NOTE: deliberately NOT using ABasePlayer::GetCrosshairTarget(). The HUD's crosshair
	// trace only queries Pawn/WorldDynamic objects, so aiming at a wall returns the far end
	// of the 10000 cm trace. Do our own trace against the level instead.
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * MaxPlaceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DecoyPlacement), false, User);

	FVector Spot = End;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// Back off the surface a little so the decoy isn't spawned inside the wall
		Spot = Hit.Location + Hit.ImpactNormal * 20.f;
	}

	// Drop it to the floor
	if (World->LineTraceSingleByChannel(Hit, Spot, Spot - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params))
	{
		Spot = Hit.Location;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = User;
	SpawnParams.Instigator = Cast<APawn>(User);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	World->SpawnActor<ADecoy>(DecoyClass, Spot, FRotator::ZeroRotator, SpawnParams);
}
