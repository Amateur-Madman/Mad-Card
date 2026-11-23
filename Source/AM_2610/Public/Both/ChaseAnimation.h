// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ChaseAnimation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoarEnded);

/**
 * 
 */
UCLASS()
class AM_2610_API UChaseAnimation : public UAnimInstance
{
	GENERATED_BODY()

public:
	void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* RoarAsset;

	FOnRoarEnded OnRoarEnded;

	void AnimRoar();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Default)
	FName ActionSlotName = "Action";

private:
	FTimerHandle RoarTimer;

	UFUNCTION()
	void RoarEnd();
};
