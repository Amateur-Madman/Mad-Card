// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WeepingAngelController.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AWeepingAngelController : public AAIController
{
	GENERATED_BODY()
protected:
	virtual void OnPossess(APawn* InPawn) override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behavior Tree")
	class UBehaviorTree* BTAsset;
};
