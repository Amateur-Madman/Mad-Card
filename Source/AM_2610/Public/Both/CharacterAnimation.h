// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CharacterAnimation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReload);

/**
 *
 */
UCLASS()
class AM_2610_API UCharacterAnimation : public UAnimInstance
{
	GENERATED_BODY()

public:
	void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;



protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;
	//set direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;
	// Set true to fire a test shot from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugFire = false;
	// Animation asset played when firing (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	class UAnimSequenceBase* FireAsset;


	// Slot the action animations plays on
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	FName SlotName = "Action";
	//set true to test hit animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugHit = false;
	// Animation asset played when hit (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	class UAnimSequenceBase* HitAsset;





	// Pool of death animations to pick from - C++ picks, Blueprint's Death state just plays whatever ends up in CurrentDeathAsset
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Death")
	TArray<class UAnimSequenceBase*> DeathAssets;

	// Result of DeathAnimation()'s random pick - read by the AnimGraph's Walking-to-Death transition rule and the Death state's Sequence Player
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Variables|Death")
	class UAnimSequenceBase* CurrentDeathAsset;

	// Set true to test death animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugDeath = false;

	// Bound to a timer sized to CurrentDeathAsset's play length - broadcasts OnDeathEnded once it fires
	UFUNCTION()
	void HandleDeathEnded();

	// Started in DeathAnimation_Implementation, cleared once HandleDeathEnded fires
	FTimerHandle DeathEndedTimerHandle;

	// Animation asset played when reloading (assigned in the Animation Blueprint)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Weapon")
	class UAnimSequenceBase* ReloadAsset;


	//set true to test hit animation from the Animation BP preview window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Debug")
	bool DebugReload = false;
	UFUNCTION(BlueprintNativeEvent)
	void PreviewWindowUpdate();
	virtual void PreviewWindowUpdate_Implementation();



public:
	// Plays the fire animation - left empty in C++ so the Animation Blueprint
	// can override it with "Play Slot Animation as Dynamic Montage".
	// Public (unlike PreviewWindowUpdate) because BP_BasePlayer/ABasePlayer
	// calls this directly on the Animation instance after Attack().
	UFUNCTION()
	void FireAnimation();


	UFUNCTION()
	void HitAnimation();


	// Renamed from Death to DeathAnimation to match the ABP_CharacterAnimation function name
	UFUNCTION(BlueprintNativeEvent, Category = "Animation")
	void DeathAnimation();
	virtual void DeathAnimation_Implementation();

	// Plays the reload animation - mirrors FireAnimation/HitAnimation/DeathAnimation
	UFUNCTION()
	void ReloadAnimation();

	UPROPERTY(BlueprintAssignable, Category = "Variables|Weapon")
	FOnReload OnReloadNow;

	UPROPERTY(BlueprintAssignable, Category = "Variables|Weapon")
	FOnReload OnReloadEnded;

	// Broadcast once CurrentDeathAsset has finished playing (timed via DeathEndedTimerHandle,
	// since a randomly-picked death asset has no reliable AnimNotify track to hook)
	UPROPERTY(BlueprintAssignable, Category = "Variables|Death")
	FOnReload OnDeathEnded;

	
};
