// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utility/Interactable.h"
#include "Cards/CardHandComponent.h"
#include "Cards/CardCarouselWidget.h"
#include "PlayerHUD.generated.h"

/**
 *
 */
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
	// Hands the hand to the card carousel, which listens to its events and draws it
	void BindCardHand(UCardHandComponent* Hand);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Asks Enhanced Input which key is currently mapped to Action ("E"); keyboard/mouse preferred
	FText GetKeyText(const class UInputAction* Action) const;

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
	UCardCarouselWidget* CardCarousel;
};