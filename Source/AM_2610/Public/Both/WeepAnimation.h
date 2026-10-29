// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "WeepAnimation.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UWeepAnimation : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<class UBlendSpace*> Movements;

	// Function to get the random blend space
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void RandomMovement();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Variables|Default")
	class UBlendSpace* CurrentMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;
};
