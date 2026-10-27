// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BaseCharacter.h"
#include "Actors/Rifle.h"
#include "Components/ChildActorComponent.h"
#include "Both/CharacterAnimation.h"
#include "Utility/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "../AM_2610.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
	// This character does not need to tick every frame.
	PrimaryActorTick.bCanEverTick = false;

	//Set skeletal mesh component to be the root component
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.f, 0.0f));
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));

	//setup child actor component for weapon socket at the hand "PlaceWeaponHere"
	WeaponComponent = CreateDefaultSubobject<UChildActorComponent>("WeaponComponent");
	WeaponComponent->SetupAttachment(GetMesh(), "PlaceWeaponHere");

	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");

	SetupCollision();

}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Tell the child actor component which class to spawn, then grab and cast the spawned actor
	WeaponComponent->SetChildActorClass(WeaponClass);

	WeaponReference = Cast<ARifle>(WeaponComponent->GetChildActor());
	if (!WeaponReference)
	{
		UE_LOG(LogTemp, Warning, TEXT("A Weapon is needed"));
	}

	// Grab and cast this character's anim instance
	AnimationReference = Cast<UCharacterAnimation>(GetMesh()->GetAnimInstance());
	if (!AnimationReference)
	{
		UE_LOG(LogTemp, Warning, TEXT("Animimation Failed"));
	}

	// Fire animation is driven BY the rifle's own successful-shot broadcast, not by input directly -
	// this keeps it in sync with the rifle's cooldown automatically (no animation plays while on cooldown)
	if (WeaponReference && AnimationReference)
	{
		WeaponReference->OnRifleAttack.AddDynamic(AnimationReference, &UCharacterAnimation::FireAnimation);
		WeaponReference->OnReloadStart.AddDynamic(AnimationReference, &UCharacterAnimation::ReloadAnimation);
	}

	if (AnimationReference)
	{
		AnimationReference->OnReloadNow.AddDynamic(this, &ABaseCharacter::HandleReloadNow);
		AnimationReference->OnReloadEnded.AddDynamic(this, &ABaseCharacter::HandleReloadEnded);
	}

	if (HealthComponent)
	{
		HealthComponent->OnHurt.AddDynamic(this, &ABaseCharacter::HandleHurt);
		HealthComponent->OnDeath.AddDynamic(this, &ABaseCharacter::HandleDeath);
	}
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}
FGenericTeamId ABaseCharacter::GetGenericTeamId() const
{
	return FGenericTeamId(TeamNumber);
}
void ABaseCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamNumber = NewTeamId.GetId();

	// Keep the controller (which is what the perception listener uses) in sync
	if (IGenericTeamAgentInterface* ControllerTeam = Cast<IGenericTeamAgentInterface>(GetController()))
	{
		ControllerTeam->SetGenericTeamId(NewTeamId);
	}
}
void ABaseCharacter::SetupCollision()
{
	// The capsule needs to OVERLAP the "Pickup" channel (so pickups can
	// detect it) while continuing to BLOCK WorldDynamic (required for
	// AProjectile's OnComponentHit to register). These are two different
	// channels on purpose - do not consolidate them onto the same one.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // "Pickup"
}
#pragma region Handles
void ABaseCharacter::HandleHurt(float Percent)
{
	if (bIsDead)
	{
		return; // no hit reaction after death
	}

	if (AnimationReference)
	{
		AnimationReference->HitAnimation(); // renamed from HitReaction to match ABP_CharacterAnimation
	}
}
void ABaseCharacter::HandleDeath(float Percent)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	if (AnimationReference)
	{
		AnimationReference->DeathAnimation();
	}
	if (WeaponReference)
	{
		WeaponReference->SetAlive(false);
	}
}
void ABaseCharacter::HandleReloadNow()
{
	if (WeaponReference)
	{
		WeaponReference->ReloadAmmo();
	}
}

void ABaseCharacter::HandleReloadEnded()
{
	if (WeaponReference)
	{
		WeaponReference->ActionStopped();
	}
}

#pragma endregion
