// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utility/Interactable.h"
#include "PlayerHUD.generated.h"

/**
 *
 */
class UBaseCard;
class UCardSlotWidget;
UCLASS()
class AM_2610_API UPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	// Bound directly to HealthComponent's OnHurt/OnDeath delegates
	UFUNCTION()
	void SetHealth(float Percent);

	// Where the crosshair's line trace currently ends (hit location, or max range if nothing hit)
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	FVector GetDestination() const { return EndPoint; }

	UFUNCTION()
	void SetAmmo(float Current, float Max);

	// Bound to ABasePlayer::OnInteractPromptChanged. Builds "Press [E] to Pick up"
	// from the prompt, or hides the text when Prompt.Target is null.
	UFUNCTION()
	void SetInteractPrompt(const FInteractPrompt& Prompt);
	// Bound to UCardHandComponent::OnCardSlotChanged / OnCardUsed. They forward to the
	// Blueprint events below so the card visuals can be built in the WBP graph.
	UFUNCTION()
	void SetCardSlot(int32 SlotIndex, UBaseCard* Card);

	UFUNCTION()
	void CardUsed(int32 SlotIndex, UBaseCard* Card);

	UFUNCTION()
	void CardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Asks Enhanced Input which key is currently mapped to Action ("E"); keyboard/mouse preferred
	FText GetKeyText(const class UInputAction* Action) const;

	// Card == null means the slot is empty
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards", meta = (DisplayName = "On Card Slot Changed"))
	void BP_OnCardSlotChanged(int32 SlotIndex, UBaseCard* Card);

	// Fires before the slot clears - play the "card spent" flash/animation here
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards", meta = (DisplayName = "On Card Used"))
	void BP_OnCardUsed(int32 SlotIndex, UBaseCard* Card);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UProgressBar* HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UImage* Crosshair;

	UPROPERTY(BlueprintReadOnly)
	class UMaterialInstanceDynamic* DynamicMaterial;

	// Must match the actual scalar/vector color parameter name on the crosshair material
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FName ColorName = "Color";

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor SafeColor = FLinearColor(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor DangerColor = FLinearColor(1.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Crosshair")
	FVector EndPoint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UTextBlock* CurrentAmmoText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UTextBlock* MaxAmmoText;

	// Optional so the existing WBP keeps compiling; add a TextBlock with this exact
	// name to WBP_PlayerHUD_New to see the prompt.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* InteractPromptText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCardSlotWidget* CardSlot_0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCardSlotWidget* CardSlot_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCardSlotWidget* CardSlot_2;

	UPROPERTY()
	TArray<UCardSlotWidget*> CardSlots;
};