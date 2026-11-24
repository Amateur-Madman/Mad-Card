// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chaser_Agent.h"
#include "Components/SphereComponent.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Both/ChaseAnimation.h"


AChaser_Agent::AChaser_Agent()
{
	HardSpeed = 60.0f;
	EasySpeed = 20.0f;
}

void AChaser_Agent::PlayerRoomStateChanged_Implementation(AActor* Player, bool bIsInRoom)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (bIsInRoom)
	{
		InitialSpeed = MoveComp->MaxWalkSpeed;
		MoveComp->MaxWalkSpeed = 0.0f;
	}
	else if (InitialSpeed > 0.0f)
	{
		MoveComp->MaxWalkSpeed = InitialSpeed + SpeedIncrement;
	}

	if (MoveComp->MaxWalkSpeed >= 550.0f)
	{
		Roared = true;
		InitialSpeed = MoveComp->MaxWalkSpeed;
		MoveComp->MaxWalkSpeed = 0.0f; 
		RoarAtSpeedThreshold();
	}
}

void AChaser_Agent::BeginPlay()
{
	Super::BeginPlay();

	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AChaser_Agent::OnProximityOverlapBegin);
	}

	if (UChaseAnimation* ChaseAnim = Cast<UChaseAnimation>(GetMesh()->GetAnimInstance()))
	{
		ChaseAnim->OnRoarEnded.AddDynamic(this, &AChaser_Agent::TrackStar);
	}
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

void AChaser_Agent::RoarAtSpeedThreshold()
{
	if (UChaseAnimation* ChaseAnim = Cast<UChaseAnimation>(GetMesh()->GetAnimInstance()))
	{
		PlayRoarSound();
		ChaseAnim->AnimRoar();
	}
}

void AChaser_Agent::TrackStar()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxWalkSpeed = InitialSpeed;
}

void AChaser_Agent::PlayRoarSound()
{

}
