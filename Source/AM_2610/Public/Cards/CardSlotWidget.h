// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Cards/BaseCard.h"
#include "CardSlotWidget.generated.h"

/**
 * One card face. Used three times by the carousel (previous / current / next).
 * Behaviour is event-driven: SetCard / PlayUsed / PlayRefused start UMG animations authored in the
 * WBP child. Every animation is optional - a missing one simply doesn't play.
 *
 * Author the animations on a panel INSIDE the slot (e.g. the root Overlay), not on the slot widget
 * itself, so any per-instance render scale / opacity set in the carousel (smaller, dimmed side
 * cards) is left alone.
 */
UCLASS()
class AM_2610_API UCardSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Card == nullptr shows the empty state
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void SetCard(UBaseCard* Card);

	// Plays the "card spent" animation on the ghost of the card's icon. Call BEFORE the card is removed.
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void PlayUsed(UBaseCard* Card);

	// Plays the "refused" animation (red flash + shake)
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void PlayRefused();

protected:
	virtual void NativePreConstruct() override;

	// Set per instance in the carousel designer (e.g. a hint for the use / cycle button)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Slot")
	FText KeyLabel;

	// "Nothing here" outline
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* EmptyFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* CardFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* CardIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* KeyText;

	// Same size/position as CardIcon; rests at opacity 0 and is only visible during UsedAnim
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* CardGhost;

	// A card arrives in an empty slot (e.g. scale 0.7 -> 1.08 -> 1.0 with a fade-in)
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> GainAnim;

	// The ghost grows and fades out (e.g. scale 1.0 -> 1.4, opacity 1 -> 0)
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> UsedAnim;

	// Frame colors flash red and fade back, with a quick decaying horizontal shake
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> RefusedAnim;

private:
	void ShowEmpty();
	void RestartAnimation(UWidgetAnimation* Anim);

	bool bHasCard = false;
};