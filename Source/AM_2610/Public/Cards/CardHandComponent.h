// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CardHandComponent.generated.h"

class UBaseCard;

// Card == nullptr means the slot is now empty.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCardSlotEvent, int32, SlotIndex, UBaseCard*, Card);

// Why a card action was refused. Drives the negative-feedback sound (and could shake the HUD slot).
UENUM(BlueprintType)
enum class ECardFeedback : uint8
{
	EmptySlot,   // pressed a slot with no card
	CannotUse,   // the card's CanUse returned false
	HandFull     // tried to add a card with no free slot
};

// Card is null for EmptySlot / HandFull. SlotIndex is INDEX_NONE for HandFull.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCardRefused, int32, SlotIndex, UBaseCard*, Card, ECardFeedback, Reason);

/**
 * The player's persistent hand: a small fixed set of slots (default 3).
 * Cards stay until the player spends them. Slot order never shifts, so the player can
 * build muscle memory ("slot 2 is always where I put Decoy").
 */
UCLASS(ClassGroup = (Cards), meta = (BlueprintSpawnableComponent))
class AM_2610_API UCardHandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCardHandComponent();

	// Puts a new card in the first empty slot. Returns false if the hand is full.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	bool TryAddCard(TSubclassOf<UBaseCard> CardClass);

	// Uses the card in SlotIndex. The card is only spent if its CanUse passed.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	bool UseSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Cards")
	bool HasSpace() const;

	UFUNCTION(BlueprintPure, Category = "Cards")
	int32 GetMaxSlots() const { return Slots.Num(); }

	// nullptr if the slot is empty or out of range
	UFUNCTION(BlueprintPure, Category = "Cards")
	UBaseCard* GetCard(int32 SlotIndex) const;

	// Re-broadcasts every slot. Call after binding a HUD, since the hand fills its starting
	// cards in BeginPlay, before the HUD exists.
	UFUNCTION(BlueprintCallable, Category = "Cards")
	void RefreshAll();

	// A slot's contents changed (card added, or card spent -> Card is null)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardSlotChanged;

	// A new card was added (NOT fired for StartingCards, so level start stays quiet)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardAdded;

	// A use / add attempt was refused (empty slot, CanUse false, hand full)
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardRefused OnCardRefused;

	// A card was successfully used. Fires BEFORE the slot clears so the HUD can animate it.
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnCardSlotEvent OnCardUsed;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MaxSlots = 3;

	// Handy for testing a card without placing a pickup
	UPROPERTY(EditDefaultsOnly, Category = "Cards")
	TArray<TSubclassOf<UBaseCard>> StartingCards;

private:
	bool AddCardInternal(TSubclassOf<UBaseCard> CardClass, bool bAnnounce);

	UPROPERTY()
	TArray<TObjectPtr<UBaseCard>> Slots;
};