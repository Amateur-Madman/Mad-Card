// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Cards/CardHandComponent.h" // ECardFeedback
#include "PlayerAudioComponent.generated.h"

class USoundBase;
class USoundClass;
class UBaseCard;

/**
 * A pool of interchangeable sounds. One is picked at random each time (never the same one
 * twice in a row). An empty array = silent, so unfilled slots are always safe.
 */
USTRUCT(BlueprintType)
struct FPlayerSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Volume = 1.f;

	// nullptr if the set has no valid sounds
	USoundBase* Pick();

	int32 LastIndex = INDEX_NONE;
};

/**
 * Every sound the player makes, in one place, so the player class stays clean.
 * Sounds are 2D (first person - they come from "you"). Fill the slots in BP_BasePlayer ->
 * PlayerAudio component. Per-card sounds (pickup / use / refused) live on each card's
 * Blueprint, not here; this component just plays them at the right moment.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class AM_2610_API UPlayerAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerAudioComponent();

	// ---- Interaction ----
	// Any successful E / F interact
	UFUNCTION(BlueprintCallable, Category = "Audio|Interact")
	void PlayInteract();

	// Pressed interact on something that refused (locked door, etc.)
	UFUNCTION(BlueprintCallable, Category = "Audio|Interact")
	void PlayInteractDenied();

	// ---- Player state ----
	UFUNCTION(BlueprintCallable, Category = "Audio|Player")
	void PlayHurt();

	UFUNCTION(BlueprintCallable, Category = "Audio|Player")
	void PlayDeath();

	UFUNCTION(BlueprintCallable, Category = "Audio|Player")
	void PlayWin();

	// An enemy just entered the player's view (Weeping Angel sting)
	UFUNCTION(BlueprintCallable, Category = "Audio|Player")
	void PlayEnemySpotted();



	// ---- Movement (not wired yet: call from anim notifies / Landed / Jump) ----
	UFUNCTION(BlueprintCallable, Category = "Audio|Movement")
	void PlayFootstep();

	UFUNCTION(BlueprintCallable, Category = "Audio|Movement")
	void PlayJump();

	UFUNCTION(BlueprintCallable, Category = "Audio|Movement")
	void PlayLand();

protected:
	virtual void BeginPlay() override;

	// Every sound this component plays is routed through this class, so the settings menu's SFX
	// slider (and Master) controls them without tagging each asset. Set it to your SFX sound class
	// in BP_BasePlayer. Leave empty to use whatever class each asset has.
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

	// Random pitch wobble (+/-) so repeats don't sound identical. 0 = off.
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.05f;

#pragma region Interact slots
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Interact")
	FPlayerSoundSet InteractSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Interact")
	FPlayerSoundSet InteractDeniedSounds;
#pragma endregion

#pragma region Card negative feedback slots
	// Pressed a card slot that has no card in it
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cards")
	FPlayerSoundSet CardEmptySlotSounds;

	// Card refused to fire (CanUse == false). Used unless that card has its own RefusedSound.
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cards")
	FPlayerSoundSet CardCannotUseSounds;

	// Tried to pick up a card with a full hand
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cards")
	FPlayerSoundSet CardHandFullSounds;
#pragma endregion

#pragma region Player state slots
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Player")
	FPlayerSoundSet HurtSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Player")
	FPlayerSoundSet DeathSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Player")
	FPlayerSoundSet WinSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Player")
	FPlayerSoundSet EnemySpottedSounds;

#pragma endregion

#pragma region Movement slots
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Movement")
	FPlayerSoundSet FootstepSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Movement")
	FPlayerSoundSet JumpSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Movement")
	FPlayerSoundSet LandSounds;
#pragma endregion

private:
	void PlaySet(FPlayerSoundSet& Set);
	void Play2D(USoundBase* Sound, float Volume = 1.f);

	// Bound to the owner's UCardHandComponent
	UFUNCTION()
	void HandleCardAdded(int32 SlotIndex, UBaseCard* Card);

	UFUNCTION()
	void HandleCardUsed(int32 SlotIndex, UBaseCard* Card);

	UFUNCTION()
	void HandleCardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason);
};