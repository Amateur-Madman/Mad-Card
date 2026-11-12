// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Cards/BaseCard.h"
#include "Cards/CardTypes.h"
#include "CardHandComponent.generated.h"

// Card == nullptr means the slot is now empty.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCardSlotEvent, int32, SlotIndex, UBaseCard*, Card);

// The selection moved. Direction is +1 (next), -1 (previous) or 0 (not a move: first card / refresh).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCardSelectionChanged, int32, SelectedIndex, UBaseCard*, Card, int32, Direction);

// Card is null for EmptySlot / HandFull. SlotIndex is INDEX_NONE for HandFull.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCardRefused, int32, SlotIndex, UBaseCard*, Card, ECardFeedback, Reason);

// The set of cards in the hand changed (card added or spent). The carousel redraws from this.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHandChanged);

/**
 * The player's hand: an ordered list of cards with one selected card (the "hotbar").
 * The player cycles the selection and uses the selected card with a single button, so the
 * hand can grow without needing more buttons. Cards are removed when spent and the selection
 * auto-advances to the card that slides into the freed position.
 */
UCLASS(ClassGroup = (Cards), meta = (BlueprintSpawnableComponent))
class AM_2610_API UCardHandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCardHandComponent();

	// Appends a card to the hand. Returns false if the hand is full.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	bool TryAddCard(TSubclassOf<UBaseCard> CardClass);

	// Moves the selection by one, wrapping around. Direction > 0 = next, < 0 = previous.
	// Does nothing with fewer than two cards.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	void CycleSelection(int32 Direction);

	// Uses the selected card. Silently ignored during the use cooldown (duplicate-press guard).
	// The card is only spent if its CanUse passed.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	bool UseSelected();

	UFUNCTION(BlueprintPure, Category = "Cards")
	bool HasSpace() const;

	UFUNCTION(BlueprintPure, Category = "Cards")
	int32 GetMaxSlots() const { return MaxSlots; }

	UFUNCTION(BlueprintPure, Category = "Cards")
	int32 GetCardCount() const { return Cards.Num(); }

	UFUNCTION(BlueprintPure, Category = "Cards")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// nullptr if the hand is empty
	UFUNCTION(BlueprintPure, Category = "Cards")
	UBaseCard* GetSelectedCard() const;

	// nullptr if the index is out of range
	UFUNCTION(BlueprintPure, Category = "Cards")
	UBaseCard* GetCard(int32 Index) const;

	// Card Offset positions away from the selection, wrapping around (-1 = previous, +1 = next).
	// For the carousel's side cards. With 2 cards, -1 and +1 are the same card; the HUD decides what to show.
	UFUNCTION(BlueprintPure, Category = "Cards")
	UBaseCard* GetCardRelativeToSelection(int32 Offset) const;

	// Re-broadcasts the hand and the selection. Call after binding a HUD, since the hand fills
	// its starting cards in BeginPlay, before the HUD exists.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	void RefreshAll();

	// The cards in the hand changed (added or spent)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnHandChanged OnHandChanged;

	// TRANSITIONAL: fires for every slot index 0..MaxSlots-1 on any hand change (Card is null past the
	// end of the hand) so the old fixed-slot HUD keeps working. Remove when the carousel replaces it.
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardSlotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSelectionChanged OnCardSelectionChanged;

	// A new card was added (NOT fired for StartingCards, so level start stays quiet)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardAdded;

	// A use / add attempt was refused (empty hand, CanUse false, hand full)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardRefused OnCardRefused;

	// A card was successfully used. Fires BEFORE the card is removed so the HUD can animate it.
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardUsed;

protected:
	virtual void BeginPlay() override;

	// Hand size cap. Raise it freely: selection is cycled, so no extra buttons are needed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards", meta = (ClampMin = "1"))
	int32 MaxSlots = 3;

	// Seconds after a successful use during which UseSelected is ignored. Guards against double
	// presses, which with auto-advance would otherwise fire the NEXT card. 0 = no cooldown.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards", meta = (ClampMin = "0.0", Units = "s"))
	float UseCooldown = 0.25f;

	// Handy for testing a card without placing a pickup
	UPROPERTY(EditDefaultsOnly, Category = "Cards")
	TArray<TSubclassOf<UBaseCard>> StartingCards;

private:
	bool AddCardInternal(TSubclassOf<UBaseCard> CardClass, bool bAnnounce);
	void NotifyHandChanged();
	void SetSelection(int32 NewIndex, int32 Direction);
	void StartUseCooldown();
	void EndUseCooldown();

	UPROPERTY()
	TArray<TObjectPtr<UBaseCard>> Cards;

	int32 SelectedIndex = 0;
	bool bUseOnCooldown = false;
	FTimerHandle UseCooldownHandle;
};	