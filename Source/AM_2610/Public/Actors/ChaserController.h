// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ChaserController.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AChaserController : public AAIController
{
	GENERATED_BODY()
protected:
	virtual void OnPossess(APawn* InPawn) override;

public:
	AChaserController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behavior Tree")
	class UBehaviorTree* BTAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Key")
	FName PlayerKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	UAIPerceptionComponent* NativePerception;

private:
	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);
};
