// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "TimerManager.h"
#include "CharacterAnimation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReload);

/**
 *
 */
UCLASS()
class AM_2610_API UCharacterAnimation : public UAnimInstance
{
	GENERATED_BODY()

	// ---------------------------------------------------------------------------
	// Public API
	// ---------------------------------------------------------------------------
public:
	void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// Plays the fire animation on SlotName.
	// Public because BP_BasePlayer/ABasePlayer calls this directly on the
	// Animation instance after Attack().
	UFUNCTION()
	void FireAnimation();

	// Plays the hit animation on SlotName.
	UFUNCTION()
	void HitAnimation();

	// Plays the reload animation on SlotName - mirrors FireAnimation/HitAnimation.
	UFUNCTION()
	void ReloadAnimation();

	// Picks a random asset from DeathAssets into CurrentDeathAsset and starts the
	// DeathEndedTimerHandle timer. Name matches the ABP_CharacterAnimation function.
	UFUNCTION(BlueprintNativeEvent, Category = "Animation")
	void DeathAnimation();
	virtual void DeathAnimation_Implementation();

	// Plays SitAsset
	UFUNCTION()
	void SitAnimation();

	// ---------------------------------------------------------------------------
	// Delegates
	// ---------------------------------------------------------------------------
public:
	UPROPERTY(BlueprintAssignable, Category = "Variables|Weapon")
	FOnReload OnReloadNow;

	UPROPERTY(BlueprintAssignable, Category = "Variables|Weapon")
	FOnReload OnReloadEnded;

	// Broadcast once CurrentDeathAsset has finished playing (timed via DeathEndedTimerHandle,
	// since a randomly-picked death asset has no reliable AnimNotify track to hook)
	UPROPERTY(BlueprintAssignable, Category = "Variables|Death")
	FOnReload OnDeathEnded;

	// Broadcast once SitAsset has finished playing (timed via SitEndedTimerHandle)
	UPROPERTY(BlueprintAssignable, Category = "Variables|Sit")
	FOnReload OnSitEnded;

	// ---------------------------------------------------------------------------
	// Locomotion
	// ---------------------------------------------------------------------------
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;

	// ---------------------------------------------------------------------------
	// Action animations (all play on SlotName)
	// ---------------------------------------------------------------------------
protected:
	// Slot the action animations play on
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	FName SlotName = "Action";

	//Slot for 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	FName FullBodySlot = "FullBody";

	// Animation asset played when firing (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	UAnimSequenceBase* FireAsset;

	// Animation asset played when hit (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	UAnimSequenceBase* HitAsset;

	// Animation asset played when reloading (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	UAnimSequenceBase* ReloadAsset;

	// ---------------------------------------------------------------------------
	// Death
	// ---------------------------------------------------------------------------
protected:
	// Pool of death animations to pick from - C++ picks, Blueprint's Death state just plays whatever ends up in CurrentDeathAsset
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Death")
	TArray<UAnimSequenceBase*> DeathAssets;

	// Result of DeathAnimation()'s random pick - read by the AnimGraph's Walking-to-Death transition rule and the Death state's Sequence Player
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Variables|Death")
	UAnimSequenceBase* CurrentDeathAsset;

	// Bound to a timer sized to CurrentDeathAsset's play length - broadcasts OnDeathEnded once it fires
	UFUNCTION()
	void HandleDeathEnded();

	// Started in DeathAnimation_Implementation, cleared once HandleDeathEnded fires
	FTimerHandle DeathEndedTimerHandle;

	// ---------------------------------------------------------------------------
	// Sit
	// ---------------------------------------------------------------------------
protected:
	// Animation asset played when sitting (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Sit")
	UAnimSequenceBase* SitAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Variables|Sit")
	bool IsSitting = false;

	UFUNCTION()
	void HandleSitEnded();

	FTimerHandle SitEndedTimerHandle;
	// ---------------------------------------------------------------------------
	// Animation BP preview window debugging
	// ---------------------------------------------------------------------------
protected:
	// Called from NativeThreadSafeUpdateAnimation when there is no owning pawn (preview window)
	UFUNCTION(BlueprintNativeEvent)
	void PreviewWindowUpdate();
	virtual void PreviewWindowUpdate_Implementation();

	// Set true to fire a test shot from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugFire = false;

	// Set true to test hit animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugHit = false;

	// Set true to test reload animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugReload = false;

	// Set true to test death animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugDeath = false;
};