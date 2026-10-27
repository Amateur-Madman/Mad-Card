// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chaser_Agent.h"
#include "Components/SphereComponent.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../AM_2610.h"


void AChaser_Agent::BeginPlay()
{
	Super::BeginPlay();

	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AChaser_Agent::OnProximityOverlapBegin);
	}
	GetCharacterMovement()->MaxWalkSpeed = 20.0f;
}

void AChaser_Agent::OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if (ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
			if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GameMode->TriggerLose();
			}
		}
	}
}

//void AChaser_Agent::PlayerRoomStateChanged(AActor* Player, bool bIsInRoom)
//{
//	if (bIsInRoom)
//	{
//		// Player entered the room, start chasing
//		if (GetCharacterMovement())
//		{
//			GetCharacterMovement()->MaxWalkSpeed = 600.0f; // Set chase speed
//		}
//	}
//	else
//	{
//		// Player left the room, stop chasing
//		if (GetCharacterMovement())
//		{
//			GetCharacterMovement()->MaxWalkSpeed = 300.0f; // Set normal speed
//		}
//	}
//}