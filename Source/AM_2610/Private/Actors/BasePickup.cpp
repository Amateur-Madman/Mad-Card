// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BasePickup.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "../AM_2610.h"
// Sets default values
ABasePickup::ABasePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	SetRootComponent(BoxCollision);
	BoxCollision->SetBoxExtent(FVector(32.f, 32.f, 32.f));

	InteractVerb = NSLOCTEXT("Pickup", "DefaultVerb", "Pick up");

	SetupCollision();

	BoxCollision->OnComponentBeginOverlap.AddDynamic(this, &ABasePickup::BoundOverlap);
}


// Called when the game starts or when spawned
void ABasePickup::BeginPlay()
{
	Super::BeginPlay();

}
void ABasePickup::SetupCollision()
{
	// Pickups live on their own custom Object Channel ("Pickup", set up in
	// Project Settings > Collision > Object Channels). This deliberately
	// avoids WorldDynamic, because ABaseCharacter's capsule BLOCKS
	// WorldDynamic (required for AProjectile's hit detection) - reusing that
	// channel here would make the capsule block pickups instead of
	// overlapping them.
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxCollision->SetCollisionObjectType(ECC_GameTraceChannel1); // "Pickup"
	BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoxCollision->SetGenerateOverlapEvents(true);
}
void ABasePickup::BoundOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("BoundOverlap fired, OtherActor = %s"), OtherActor ? *OtherActor->GetName() : TEXT("null"));

	// Press-to-interact pickups are collected through Interact(), never by walking over them.
	if (bRequiresInteract)
	{
		return;
	}

	// The player's InteractionSphere also overlaps this box (that is how the player
	// finds interactables). Only the character's capsule should trigger auto-pickup,
	// otherwise pickups would be grabbed from the sphere's radius instead of on contact.
	if (const ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (OtherComp != Character->GetCapsuleComponent())
		{
			return;
		}
	}

	Collect(OtherActor, SweepResult);
}

void ABasePickup::Collect(AActor* Collector, const FHitResult& SweepResult)
{
	if (!Execute_CanPickup(this, Collector))
	{
		return;
	}

	HandlePickup(Collector, SweepResult);
	PostPickup();
}

void ABasePickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	// Intentionally empty - overridden per pickup type
}

void ABasePickup::PostPickup()
{
	Destroy();
}

bool ABasePickup::IsPlayerPawn(AActor* Target) const
{
	const APawn* Pawn = Cast<APawn>(Target);
	return Pawn && Pawn->IsPlayerControlled();
}

// ---------------------------------------------------------------------------
// IInteractable
// ---------------------------------------------------------------------------
bool ABasePickup::CanInteract_Implementation(AActor* Interactor)
{
	// CanPickup stays the single source of truth for eligibility, so a subclass that
	// says "no" (e.g. health pickup at full health) also hides the prompt.
	return bRequiresInteract && Execute_CanPickup(this, Interactor);
}

FInteractPrompt ABasePickup::GetInteractPrompt_Implementation(AActor* Interactor)
{
	FInteractPrompt Prompt;
	Prompt.Slot = InteractSlot;
	Prompt.Verb = InteractVerb;
	Prompt.Target = this;
	return Prompt;
}

void ABasePickup::Interact_Implementation(AActor* Interactor)
{
	if (!bRequiresInteract)
	{
		return;
	}

	// No sweep happens on this path, so SweepResult is empty. If a subclass's
	// HandlePickup reads SweepResult (impact point etc.), guard for that.
	Collect(Interactor, FHitResult());
}