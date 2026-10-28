// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BasePickup.h"
#include "Cards/BaseCard.h"
#include "CardPickup.generated.h"

class UBillboardComponent;
class UStaticMeshComponent;
class UTexture2D;

/**
 * Picks a card up into the player's hand. Press-to-interact by default (E), and the prompt
 * hides itself while the hand is full because CanInteract is driven by CanPickup.
 */
UCLASS()
class AM_2610_API ACardPickup : public ABasePickup
{
	GENERATED_BODY()

public:
	ACardPickup();

	virtual bool CanPickup_Implementation(AActor* Target) override;

	// Read the icon from the class already selected for this pickup.
	UFUNCTION(BlueprintPure, Category = "Pickup|Display")
	virtual UTexture2D* GetPickupIcon() const;

	// Called after the startup roll. Call again if Blueprint changes the reward later.
	UFUNCTION(BlueprintCallable, Category = "Pickup|Display")
	void RefreshPickupVisual();

protected:
	virtual void BeginPlay() override;
	virtual void PostPickup() override;
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;

	// Which card this pickup grants (e.g. BP_DashCard, BP_DecoyCard)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Card")
	TSubclassOf<UBaseCard> CardClass;

	// Choose one valid class when spawned. Leave empty to use CardClass.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Card")
	TArray<TSubclassOf<UBaseCard>> RandomCardClasses;

	// Keep this pickup available after collecting a card. The hand still needs space.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Card")
	bool bInfinitePickups = false;

	// Visual only - collision is the root box from ABasePickup
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Card")
	UStaticMeshComponent* CardMesh;

	// World-space icon. Move/scale this inherited component in the pickup Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Display")
	TObjectPtr<UBillboardComponent> CardIconDisplay;
};