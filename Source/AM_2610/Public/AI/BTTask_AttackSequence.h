// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_AttackSequence.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UBTTask_AttackSequence : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_AttackSequence();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
