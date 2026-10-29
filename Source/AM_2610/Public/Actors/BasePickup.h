// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utility/PickupQueries.h"
#include "Utility/Interactable.h"
#include "BasePickup.generated.h"

UCLASS(Abstract)
class AM_2610_API ABasePickup : public AActor
	, public IPickupQueries
	, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABasePickup();

	// ---- IInteractable ----
	virtual bool CanInteract_Implementation(AActor* Interactor) override;
	virtual FInteractPrompt GetInteractPrompt_Implementation(AActor* Interactor) override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Root collision volume - overlap triggers the pickup
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	class UBoxComponent* BoxCollision;

	// false = collected automatically when the player's capsule overlaps (old behavior).
	// true  = player must press the context button while focused on it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Interaction")
	bool bRequiresInteract = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Interaction", meta = (EditCondition = "bRequiresInteract"))
	EInteractSlot InteractSlot = EInteractSlot::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Interaction", meta = (EditCondition = "bRequiresInteract"))
	FText InteractVerb;

	// Bound to BoxCollision's OnComponentBeginOverlap in the constructor
	UFUNCTION()
	void BoundOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult);
	virtual void PostPickup();
	// Shared by any pickup type whose CanPickup_Implementation restricts itself to the
	// player only (e.g. AHealthPickup, AAmmoPickup) 
	bool IsPlayerPawn(AActor* Target) const;

private:
	// Single source of truth for pickup collision setup - called from the
	// constructor. Kept as its own function so the intent (pickups sit on
	// their own Object Channel, "Pickup", and only overlap Pawns) is easy to
	// find and re-verify later without wading through the whole constructor.
	void SetupCollision();

	// Shared by the auto-pickup overlap path and the Interact path so both
	// run the exact same CanPickup -> HandlePickup -> PostPickup sequence.
	void Collect(AActor* Collector, const FHitResult& SweepResult);
};