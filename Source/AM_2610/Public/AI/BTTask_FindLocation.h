// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindLocation.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UBTTask_FindLocation : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_FindLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	// Blackboard key (Vector) the found location is written to
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LocationKey;

	// Search radius around the controlled pawn to find a navigable point within
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	float Radius = 3000.0f;
};
