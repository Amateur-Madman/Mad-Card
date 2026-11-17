// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/CardHandComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCardHandComponent::UCardHandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCardHandComponent::BeginPlay()
{
	Super::BeginPlay();

	Cards.Reset();
	Cards.Reserve(FMath::Max(MaxSlots, 1));
	SelectedIndex = 0;

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

	if (Cards.Num() >= MaxSlots)
	{
		if (bAnnounce)
		{
			OnCardRefused.Broadcast(INDEX_NONE, nullptr, ECardFeedback::HandFull);
		}
		return false;
	}

	// Outer = the owning actor, so UBaseCard::GetWorld() resolves
	UBaseCard* NewCard = NewObject<UBaseCard>(GetOwner(), CardClass);
	const int32 NewIndex = Cards.Add(NewCard);

	NotifyHandChanged();
	if (bAnnounce)
	{
		OnCardAdded.Broadcast(NewIndex, NewCard);
	}

	// The first card into an empty hand becomes the selection
	if (Cards.Num() == 1)
	{
		SetSelection(0, 0);
	}
	return true;
}

void UCardHandComponent::CycleSelection(int32 Direction)
{
	const int32 Count = Cards.Num();
	if (Count < 2 || Direction == 0)
	{
		return; // nothing to cycle to
	}

	const int32 Step = FMath::Sign(Direction);
	SetSelection((SelectedIndex + Step + Count) % Count, Step);
	OnCardCycled.Broadcast(Step);
}

bool UCardHandComponent::UseSelected()
{
	// Duplicate-press guard. Silent on purpose: a double tap should not look like an error.
	if (bUseOnCooldown)
	{
		return false;
	}

	if (!Cards.IsValidIndex(SelectedIndex))
	{
		OnCardRefused.Broadcast(SelectedIndex, nullptr, ECardFeedback::EmptySlot);
		return false;
	}

	UBaseCard* Card = Cards[SelectedIndex];

	// A refused use (CanUse == false) keeps the card and does NOT start the cooldown
	if (!Card->TryUse(GetOwner()))
	{
		OnCardRefused.Broadcast(SelectedIndex, Card, ECardFeedback::CannotUse);
		return false;
	}

	const int32 UsedIndex = SelectedIndex;
	OnCardUsed.Broadcast(UsedIndex, Card);
	StartUseCooldown();

	if (Card->IsConsumedOnUse())
	{
		Cards.RemoveAt(UsedIndex);
		NotifyHandChanged();

		// Auto-advance: the next card slid into UsedIndex. Spending the last card wraps to the first.
		if (!Cards.IsEmpty())
		{
			SetSelection(UsedIndex % Cards.Num(), +1);
		}
		else
		{
			SelectedIndex = 0;
		}
	}

	return true;
}

void UCardHandComponent::SetSelection(int32 NewIndex, int32 Direction)
{
	SelectedIndex = NewIndex;
	OnCardSelectionChanged.Broadcast(SelectedIndex, GetSelectedCard(), Direction);
}

void UCardHandComponent::NotifyHandChanged()
{
	OnHandChanged.Broadcast();
}

void UCardHandComponent::StartUseCooldown()
{
	UWorld* World = GetWorld();
	if (UseCooldown <= 0.f || !World)
	{
		return;
	}

	bUseOnCooldown = true;
	World->GetTimerManager().SetTimer(UseCooldownHandle, this, &UCardHandComponent::EndUseCooldown, UseCooldown, false);
}

void UCardHandComponent::EndUseCooldown()
{
	bUseOnCooldown = false;
}

bool UCardHandComponent::HasSpace() const
{
	return Cards.Num() < MaxSlots;
}

UBaseCard* UCardHandComponent::GetSelectedCard() const
{
	return GetCard(SelectedIndex);
}

UBaseCard* UCardHandComponent::GetCard(int32 Index) const
{
	return Cards.IsValidIndex(Index) ? Cards[Index].Get() : nullptr;
}

UBaseCard* UCardHandComponent::GetCardRelativeToSelection(int32 Offset) const
{
	const int32 Count = Cards.Num();
	if (Count == 0)
	{
		return nullptr;
	}

	const int32 Index = ((SelectedIndex + Offset) % Count + Count) % Count;
	return Cards[Index];
}

void UCardHandComponent::RefreshAll()
{
	NotifyHandChanged();

	if (UBaseCard* Card = GetSelectedCard())
	{
		OnCardSelectionChanged.Broadcast(SelectedIndex, Card, 0);
	}
}
