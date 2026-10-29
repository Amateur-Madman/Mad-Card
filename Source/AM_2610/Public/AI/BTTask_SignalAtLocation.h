// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SignalAtLocation.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UBTTask_SignalAtLocation : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_SignalAtLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
