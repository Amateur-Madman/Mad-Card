// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardSlotWidget.generated.h"

class UBaseCard;
class UBorder;
class UImage;
class UTextBlock;

/**
 * One slot of the card bar. All behaviour (show/hide, pop-in, "card spent" flash) is in C++, so
 * the WBP child only needs the layout with the right widget names - no graph, no UMG animations.
 */
UCLASS()
class AM_2610_API UCardSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Card == nullptr shows the empty state
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void SetCard(UBaseCard* Card);

	// Flashes a ghost of the card's icon growing and fading. Call BEFORE the slot clears.
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void PlayUsed(UBaseCard* Card);

	// Red flash + small shake. Called when a use attempt is refused.
	UFUNCTION(BlueprintCallable, Category = "Card Slot")
	void PlayRefused();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Set per instance in the HUD designer ("1", "2", "3")
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

	// Same size/position as CardIcon; only visible during the "used" flash
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* CardGhost;

	// Pop-in length (seconds) when a card arrives in an empty slot
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float GainDuration = 0.25f;

	// Length of the "used" flash
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float UsedDuration = 0.2f;

	// How big the ghost grows by the end of the flash
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float GhostEndScale = 1.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float RefusedDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	FLinearColor RefusedColor = FLinearColor(1.f, 0.1f, 0.1f, 1.f);

	// Horizontal shake in pixels; 0 = flash only
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float RefusedShake = 6.f;

private:
	void ShowEmpty();

	bool bHasCard = false;

	// Elapsed time of each effect; == duration means "not playing"
	float GainElapsed = 0.f;
	float UsedElapsed = 0.f;

	void ApplyRefused(float Alpha, float ShakeX);

	FLinearColor EmptyBaseColor = FLinearColor::White;
	FLinearColor CardBaseColor = FLinearColor::White;
	float RefusedElapsed = 0.f;
};
