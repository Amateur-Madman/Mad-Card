// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/CardPickup.h"
#include "Cards/CardHandComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"

ACardPickup::ACardPickup()
{
	CardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh"));
	CardMesh->SetupAttachment(GetRootComponent());
	CardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CardIconDisplay = CreateDefaultSubobject<UBillboardComponent>(TEXT("CardIconDisplay"));
	CardIconDisplay->SetupAttachment(GetRootComponent());
	CardIconDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CardIconDisplay->SetHiddenInGame(false);
	CardIconDisplay->SetSprite(nullptr);
	CardIconDisplay->bIsScreenSizeScaled = false;
	CardIconDisplay->bUseInEditorScaling = false;
	CardIconDisplay->SetRelativeScale3D(FVector(0.1f));

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

	// MazeCardPickup has already selected Second Chance before calling this BeginPlay.
	// Virtual GetPickupIcon() therefore resolves the icon for the actual reward.
	RefreshPickupVisual();
}

UTexture2D* ACardPickup::GetPickupIcon() const
{
	const UBaseCard* CardDefaults = CardClass ? CardClass->GetDefaultObject<UBaseCard>() : nullptr;
	return CardDefaults ? CardDefaults->GetIcon() : nullptr;
}

void ACardPickup::RefreshPickupVisual()
{
	if (!IsValid(CardIconDisplay))
	{
		return;
	}

	UTexture2D* PickupIcon = GetPickupIcon();
	CardIconDisplay->SetSprite(PickupIcon);
	CardIconDisplay->SetVisibility(PickupIcon != nullptr);
}

void ACardPickup::PostPickup()
{
	// Puzzle pickups supply another card each time without removing the display.
	if (!bInfinitePickups)
	{
		Super::PostPickup();
	}
}