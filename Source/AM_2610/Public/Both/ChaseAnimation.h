// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ChaseAnimation.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UChaseAnimation : public UAnimInstance
{
	GENERATED_BODY()

public:
	void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;
};
