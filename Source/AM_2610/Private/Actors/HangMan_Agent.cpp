// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/HangMan_Agent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Both/HangAnimation.h"
#include "Actors/BasePlayer.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Actors/HangManController.h"
#include "../AM_2610.h"

AHangMan_Agent::AHangMan_Agent()
{
	DetectionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("DetectionComponent"));
	if (DetectionComponent)
	{
		DetectionComponent->SetupAttachment(GetRootComponent());
	}
}

void AHangMan_Agent::BeginPlay()
{
	Super::BeginPlay();

	//Inverse gravity by default
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetGravityDirection(FVector(0.0f, 0.0f, 1.0f));
	}

	if (DetectionComponent)
	{
		DetectionComponent->OnComponentBeginOverlap.AddDynamic(this, &AHangMan_Agent::OnDetectionOverlapBegin);
	}

	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AHangMan_Agent::OnProximityOverlapBegin);
	}

	if (UHangAnimation* HangAnim = Cast<UHangAnimation>(GetMesh()->GetAnimInstance()))
	{
		HangAnim->OnFalling.AddDynamic(this, &AHangMan_Agent::Fall);
		HangAnim->OnFlying.AddDynamic(this, &AHangMan_Agent::Fly);
	}

	OnAttackEnded.AddDynamic(this, &AHangMan_Agent::ReleasePlayer);
}

//Check if the agent is on the ceiling or floor
void AHangMan_Agent::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (UHangAnimation* HangAnim = Cast<UHangAnimation>(GetMesh()->GetAnimInstance()))
	{
		if (GetCharacterMovement()->IsWalking() && GetCharacterMovement()->GetGravityDirection() == FVector(0.0f, 0.0f, 1.0f) && HangAnim->GetFlying())
		{
			HangAnim->OnHangAround.Broadcast();
			DetectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ProximitySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}

		if (GetCharacterMovement()->IsWalking() && GetCharacterMovement()->GetGravityDirection() == FVector(0.0f, 0.0f, -1.0f) && HangAnim->GetAttacking())
		{
			HangAnim->OnHangAround.Broadcast();
			if (AHangManController* AIC = Cast<AHangManController>(GetController()))
			{
				if (UBlackboardComponent* BBComp = AIC->GetBlackboardComponent())
				{
					BBComp->SetValueAsBool(TEXT("Attacking"), true);
				}
			}
		}
	}
}

//Detected player, trigger falling animation and normal gravity
void AHangMan_Agent::OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if(ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
			if (UHangAnimation* HangAnim = Cast<UHangAnimation>(GetMesh()->GetAnimInstance()))
			{
				HangAnim->AnimFall();
			}
			DetectionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

//Detected player in proximity, attach player to agent and disable inputs
void AHangMan_Agent::OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if (ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
			CaughtPlayer = OtherCharacter;
			CaughtPlayer->SetActorLocation(ChildActorComponent->GetComponentLocation());
			OtherCharacter->BeginViewLock(this, FVector(0.0f, 0.0f, 60.0f));
			//OtherCharacter->GetCharacterMovement()->DisableMovement();
			//OtherCharacter->Camera->SetActorRelativeRotation(ChildActorComponent->GetComponentRotation());
			//if (APlayerController* PlayerController = Cast<APlayerController>(OtherCharacter->GetController()))
			//{
			//	PlayerController->SetIgnoreMoveInput(true);
			//	//PlayerController->SetIgnoreLookInput(true);
			//	//PlayerController->PlayerCameraManager->SetActorRelativeRotation(ChildActorComponent->GetComponentRotation());
			//}
			ProximitySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

//Normal gravity to get off of ceiling
void AHangMan_Agent::Fall()
{
	GetCharacterMovement()->SetGravityDirection(FVector(0.0f, 0.0f, -1.0f));
}

//Invert gravity to get onto ceiling
void AHangMan_Agent::Fly()
{
	GetCharacterMovement()->SetGravityDirection(FVector(0.0f, 0.0f, 1.0f));
}

void AHangMan_Agent::ReleasePlayer()
{
	if (CaughtPlayer)
	{
		CaughtPlayer->EndViewLock();
		CaughtPlayer = nullptr;
	}
}
