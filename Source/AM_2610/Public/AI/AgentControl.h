// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AgentControl.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AAgentControl : public AAIController
{
	GENERATED_BODY()
	
public:
	AAgentControl();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// Behavior tree this controller runs when it possesses a pawn
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	class UBehaviorTree* BTAsset;

	// Blackboard key set/cleared with the sensed player actor
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	FName PlayerKeyName = "Player";

	UPROPERTY(VisibleAnywhere, Category = "AI")
	class UAIPerceptionComponent* AIPerception;
};
