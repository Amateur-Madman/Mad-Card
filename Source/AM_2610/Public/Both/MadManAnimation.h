// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MadManAnimation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAnimOver);

/**
 * 
 */
UCLASS()
class AM_2610_API UMadManAnimation : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* WaveAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* PointAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Default)
	FName ActionSlotName = "Action";

	FOnAnimOver OnPointOver;

	UFUNCTION()
	void AnimWave();

	UFUNCTION()
	void AnimPoint();

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;

private:
	FTimerHandle AnimTimerHandle;
};