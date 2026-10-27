// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BaseCard.generated.h"

class UTexture2D;
class USoundBase;

/**
 * A card is a verb the player can perform in the 3D world (Dash, Decoy, Flare...).
 * One C++/Blueprint subclass per card type; the hand (UCardHandComponent) owns instances.
 *
 * To add a card: subclass this, override CanUse_Implementation / Use_Implementation,
 * then make a Blueprint child to set the name/icon/tuning numbers.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class AM_2610_API UBaseCard : public UObject
{
	GENERATED_BODY()

public:
	// Called by UCardHandComponent. Returns true only if the card was actually used.
	// The hand only spends the card on true, so a refused use (CanUse == false) never wastes it.
	bool TryUse(AActor* User);

	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Card")
	UTexture2D* GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Card")
	bool IsConsumedOnUse() const { return bConsumedOnUse; }

	// Played when the card lands in the hand / is used / refuses to fire. Null = silent.
	UFUNCTION(BlueprintPure, Category = "Card|Audio")
	USoundBase* GetPickupSound() const { return PickupSound; }

	UFUNCTION(BlueprintPure, Category = "Card|Audio")
	USoundBase* GetUseSound() const { return UseSound; }

	UFUNCTION(BlueprintPure, Category = "Card|Audio")
	USoundBase* GetRefusedSound() const { return RefusedSound; }

	// Cards are UObjects, so give them a world through their outer (the owning actor).
	// Lets Blueprint children use latency/timer/spawn nodes.
	virtual UWorld* GetWorld() const override;

protected:
	// Is the card usable right now? Return false to refuse without spending it
	// (e.g. Dash while already dashing).
	UFUNCTION(BlueprintNativeEvent, Category = "Card")
	bool CanUse(AActor* User) const;
	virtual bool CanUse_Implementation(AActor* User) const;

	// The card's effect. User is the actor that owns the hand (the player).
	UFUNCTION(BlueprintNativeEvent, Category = "Card")
	void Use(AActor* User);
	virtual void Use_Implementation(AActor* User);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TObjectPtr<UTexture2D> Icon;

	// true = the slot frees up after a successful use. Kept as a flag so a rechargeable
	// card can be tested later without touching the hand.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	bool bConsumedOnUse = true;

	// Set these in each card's Blueprint child (BP_DashCard, BP_DecoyCard...)
	// Picking this card up (e.g. a whoosh-y paper slide for Dash)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Audio")
	TObjectPtr<USoundBase> PickupSound;

	// The card's effect firing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Audio")
	TObjectPtr<USoundBase> UseSound;

	// Optional: this card's own "can't use right now" sound. If empty, the player's generic
	// CardCannotUseSounds play instead.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card|Audio")
	TObjectPtr<USoundBase> RefusedSound;
};