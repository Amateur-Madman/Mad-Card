// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/CardPickup.h"
#include "Cards/CardHandComponent.h"
#include "Components/StaticMeshComponent.h"

ACardPickup::ACardPickup()
{
	CardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh"));
	CardMesh->SetupAttachment(GetRootComponent());
	CardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Cards are deliberate pickups: the player presses the context button, and the choice
	// to leave one behind is part of the risk/reward loop.
	bRequiresInteract = true;
	InteractVerb = NSLOCTEXT("CardPickup", "TakeCard", "Take card");
}

bool ACardPickup::CanPickup_Implementation(AActor* Target)
{
	if (!CardClass || !IsPlayerPawn(Target))
	{
		return false;
	}

	const UCardHandComponent* Hand = Target->FindComponentByClass<UCardHandComponent>();
	return Hand && Hand->HasSpace();
}

void ACardPickup::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (UCardHandComponent* Hand = OtherActor ? OtherActor->FindComponentByClass<UCardHandComponent>() : nullptr)
	{
		Hand->TryAddCard(CardClass);
	}
}

void ACardPickup::BeginPlay()
{
	TArray<TSubclassOf<UBaseCard>> AvailableCards = RandomCardClasses;
	AvailableCards.RemoveAll([](const TSubclassOf<UBaseCard>& Entry)
		{
			return !Entry;
		});

	// Roll once so looking at the pickup does not keep changing its card.
	if (!AvailableCards.IsEmpty())
	{
		CardClass = AvailableCards[FMath::RandRange(0, AvailableCards.Num() - 1)];
	}

	Super::BeginPlay();
}

void ACardPickup::PostPickup()
{
	// Puzzle pickups supply another card each time without removing the display.
	if (!bInfinitePickups)
	{
		Super::PostPickup();
	}
}