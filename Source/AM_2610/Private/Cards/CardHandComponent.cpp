// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/CardHandComponent.h"
#include "Cards/BaseCard.h"

UCardHandComponent::UCardHandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCardHandComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.Reset();
	Slots.SetNum(FMath::Max(MaxSlots, 1));

	for (const TSubclassOf<UBaseCard>& CardClass : StartingCards)
	{
		AddCardInternal(CardClass, /*bAnnounce=*/false); // no pickup sounds at level start
	}
}

bool UCardHandComponent::TryAddCard(TSubclassOf<UBaseCard> CardClass)
{
	return AddCardInternal(CardClass, /*bAnnounce=*/true);
}

bool UCardHandComponent::AddCardInternal(TSubclassOf<UBaseCard> CardClass, bool bAnnounce)
{
	if (!CardClass)
	{
		return false;
	}

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i])
		{
			// Outer = the owning actor, so UBaseCard::GetWorld() resolves
			Slots[i] = NewObject<UBaseCard>(GetOwner(), CardClass);
			OnCardSlotChanged.Broadcast(i, Slots[i]);
			if (bAnnounce)
			{
				OnCardAdded.Broadcast(i, Slots[i]);
			}
			return true;
		}
	}

	if (bAnnounce)
	{
		OnCardRefused.Broadcast(INDEX_NONE, nullptr, ECardFeedback::HandFull);
	}
	return false; // hand full
}

bool UCardHandComponent::UseSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false; // not a real slot (more input actions than slots) - stay silent
	}

	if (!Slots[SlotIndex])
	{
		OnCardRefused.Broadcast(SlotIndex, nullptr, ECardFeedback::EmptySlot);
		return false;
	}

	UBaseCard* Card = Slots[SlotIndex];

	// A refused use (CanUse == false) keeps the card
	if (!Card->TryUse(GetOwner()))
	{
		OnCardRefused.Broadcast(SlotIndex, Card, ECardFeedback::CannotUse);
		return false;
	}

	OnCardUsed.Broadcast(SlotIndex, Card);

	if (Card->IsConsumedOnUse())
	{
		Slots[SlotIndex] = nullptr;
		OnCardSlotChanged.Broadcast(SlotIndex, nullptr);
	}

	return true;
}

bool UCardHandComponent::HasSpace() const
{
	for (const TObjectPtr<UBaseCard>& Card : Slots)
	{
		if (!Card)
		{
			return true;
		}
	}
	return false;
}

UBaseCard* UCardHandComponent::GetCard(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void UCardHandComponent::RefreshAll()
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		OnCardSlotChanged.Broadcast(i, Slots[i]);
	}
}