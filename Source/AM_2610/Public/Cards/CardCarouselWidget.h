// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Cards/BaseCard.h"
#include "Cards/CardHandComponent.h"
#include "Cards/CardSlotWidget.h"
#include "Cards/CardTypes.h"
#include "CardCarouselWidget.generated.h"

/**
 * The card hotbar as a carousel: the selected card large in the middle, its neighbors on each side.
 * The footprint stays the same at 2 cards or 8, and the selected card is always in the same place,
 * so every card VFX is authored once on CurrentSlot.
 *
 * It binds to the hand's delegates itself (BindToHand), so nothing else needs to know how it is built.
 */
UCLASS()
class AM_2610_API UCardCarouselWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Starts listening to the hand and draws its current contents. Safe to call again with another hand.
	void BindToHand(UCardHandComponent* InHand);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCardSlotWidget* PrevSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCardSlotWidget* CurrentSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCardSlotWidget* NextSlot;

	// "2/5" style counter, blank with fewer than two cards
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CounterText;

	// Selection moved to the next card: animate the row sliding in from the right (translation X +n -> 0)
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlideNextAnim;

	// Selection moved to the previous card: slide in from the left (translation X -n -> 0)
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlidePrevAnim;

private:
	UFUNCTION()
	void HandleHandChanged();

	UFUNCTION()
	void HandleSelectionChanged(int32 SelectedIndex, UBaseCard* Card, int32 Direction);

	UFUNCTION()
	void HandleCardUsed(int32 SlotIndex, UBaseCard* Card);

	UFUNCTION()
	void HandleCardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason);

	void UnbindFromHand();
	void Redraw();
	void RestartAnimation(UWidgetAnimation* Anim);

	TWeakObjectPtr<UCardHandComponent> Hand;
};
