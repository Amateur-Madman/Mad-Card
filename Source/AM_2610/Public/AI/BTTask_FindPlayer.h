// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindPlayer.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UBTTask_FindPlayer : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_FindPlayer();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PlayerKey;
	
private:
	class UAITargetStack* TargetStack;
};
