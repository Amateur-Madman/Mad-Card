// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MadManAnimation.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UMadManAnimation : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;
};
