// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BaseCharacter.h"
#include "Utility/Interactable.h"
#include "InputActionValue.h"
#include "Perception/AIPerceptionTypes.h"
#include "BasePlayer.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBroadcast);

// Fired whenever the focused interactable (or its prompt) changes.
// Prompt.Target == nullptr means "hide the prompt".
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractPromptChanged, const FInteractPrompt&, Prompt);
/**
 *
 */
UCLASS()
class AM_2610_API ABasePlayer : public ABaseCharacter
{
	GENERATED_BODY()

	// Layout: one #pragma region per system. Inside a region there is at most one
	// public / protected / private block, always in that order. 

#pragma region Core
public:
	ABasePlayer();

protected:
	virtual void BeginPlay() override;

	// Cached in BeginPlay - reused by HUD creation, the view lock and PlayerLost/PlayerWon
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
	class APlayerController* ThePlayerController;
#pragma endregion

#pragma region Camera and Look Detection
protected:
	UPROPERTY(VisibleAnywhere, Category = Character)
	class UCameraComponent* Camera;

	// Sight-only perception on the player. Enemies that implement IEnemyBehaviorInterface
	// (Weeping Angel) are told via PlayerSeesMe when they enter/leave the player's view.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perception")
	class UAIPerceptionComponent* CameraPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perception")
	class UAISenseConfig_Sight* SightConfig;

	UFUNCTION()
	void OnPlayerSpottedActor(AActor* Actor, FAIStimulus Stimulus);
#pragma endregion

#pragma region Input
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// ---- Enhanced Input assets (assign in BP_BasePlayer's class defaults) ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	// Axis2D: X = strafe (A/D), Y = forward (W/S)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* MoveAction;

	// Axis2D: X = yaw, Y = pitch (mouse XY)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* LookAction;

	// Context buttons: Primary (E) and Secondary (F). Interactables pick a slot.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* InteractPrimaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* InteractSecondaryAction;

	// One Input Action per hand slot, in slot order (IA_Card1, IA_Card2, IA_Card3)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<class UInputAction*> CardSlotActions;

private:
	// Enhanced Input handlers
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void InputInteract1();
	void InputInteract2();
	void InputUseCard(int32 SlotIndex);
#pragma endregion

#pragma region Audio
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	class UPlayerAudioComponent* PlayerAudio;
#pragma endregion

#pragma region Interaction
public:
	// Broadcast when the "Press [] to {}" prompt should change or hide
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractPromptChanged OnInteractPromptChanged;

protected:
	// Proximity: anything with an overlapping "Pickup"-channel volume that implements
	// IInteractable becomes a candidate while inside this sphere.
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	class USphereComponent* InteractionSphere;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRadius = 200.f;

	// Focus: a candidate is only focusable if it lies within this distance (cm) of the
	// camera's aim ray, i.e. roughly "under the crosshair". Closest to the ray wins.
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float AimTolerance = 120.f;

	// How often focus is re-evaluated while at least one candidate is in range
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float FocusUpdateInterval = 0.05f;

private:
	UFUNCTION()
	void OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void TryInteract(EInteractSlot Slot);
	void UpdateFocus();
	void SetFocusedInteractable(AActor* NewFocus);
	void ClearInteraction();

	// Not UPROPERTY on purpose: weak pointers, so a pickup destroying itself can't dangle
	TArray<TWeakObjectPtr<AActor>> InteractCandidates;
	TWeakObjectPtr<AActor> FocusedInteractable;
	FInteractPrompt CurrentPrompt;
	FTimerHandle FocusTimerHandle;
#pragma endregion

#pragma region Cards
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cards")
	class UCardHandComponent* CardHand;
	
#pragma endregion

#pragma region Combat
public:
	// Increases the rifle's max ammo capacity by Amount, updating the HUD's max number
	// without changing CurrentAmmo - called by AAmmoPickup on pickup
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AddMaxAmmo(float Amount);

protected:
	virtual void HandleHurt(float Percent) override;
	virtual void HandleDeath(float Percent) override;
	UFUNCTION()
	void HandleAmmoChanged(float Current, float Max);
#pragma endregion

#pragma region HUD
public:
	// Returns the exact world point the player's crosshair is currently over,
	// so ARifle::Attack() aims at precisely what the reticle shows.
	UFUNCTION(BlueprintPure, Category = "HUD")
	FVector GetCrosshairTarget() const;

protected:
	// Soft reference - only resolved/loaded when actually needed, not at construction time
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<class UPlayerHUD> HUDClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD")
	class UPlayerHUD* HUDObject;
#pragma endregion

#pragma region Game Flow
public:
	UFUNCTION()
	void PlayerWon();

	UFUNCTION()
	void FreezeForResults();

	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnBroadcast OnPlayerLost;

private:
	// Bound to AnimationReference's OnDeathEnded - broadcasts OnPlayerLost, shows the
	// mouse cursor, and removes the HUD once the death animation finishes playing
	UFUNCTION()
	void PlayerLost();
#pragma endregion

#pragma region Grapple View Lock
public:
	// Smoothly turns the player's view toward Target (+ TargetOffset) and ignores look input
	// until EndViewLock(). Rotates the controller, so the camera stays private.
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void BeginViewLock(AActor* Target, FVector TargetOffset = FVector::ZeroVector, float InterpSpeed = 6.f);

	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void EndViewLock();

	// Only ticks while a view lock is active
	virtual void Tick(float DeltaTime) override;

private:
	bool bViewLocked = false;
	TWeakObjectPtr<AActor> ViewLockTarget;
	FVector ViewLockOffset = FVector::ZeroVector;
	float ViewLockSpeed = 6.f;
#pragma endregion
};