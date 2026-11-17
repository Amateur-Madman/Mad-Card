// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BaseEnemyController.generated.h"

/**
 * Shared base for every enemy controller. Runs the Behavior Tree on possess and keeps the
 * Blackboard's PlayerKey in sync with UAITargetStack (player / decoy). The stack pushes changes
 * to us through a delegate, so nothing here needs Tick.
 */
UCLASS()
class AM_2610_API ABaseEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behavior Tree")
	UBehaviorTree* BTAsset = nullptr;

	// Name of the Object key on the Blackboard that holds "who to chase"
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Key")
	FName PlayerKey = TEXT("Player");

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Called whenever the top of the target stack changes. Override in a child to customize.
	virtual void HandleTargetChanged(AActor* NewTarget);

private:
	FDelegateHandle TargetChangedHandle;
};
