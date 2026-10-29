// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_SignalAtLocation.h"
#include "AIController.h"
#include "Actors/HangMan_Agent.h"
#include "Both/HangAnimation.h"

UBTTask_SignalAtLocation::UBTTask_SignalAtLocation()
{
	NodeName = TEXT("Signal At Location");
}

EBTNodeResult::Type UBTTask_SignalAtLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	if (ControlledPawn->IsA(AHangMan_Agent::StaticClass()))
	{
		AHangMan_Agent* HangManAgent = Cast<AHangMan_Agent>(ControlledPawn);
		if (HangManAgent)
		{
			UHangAnimation* HangAnim = Cast<UHangAnimation>(HangManAgent->GetMesh()->GetAnimInstance());
			if (HangAnim)
			{
				HangAnim->OnNewLocation.Broadcast();
			}
		}
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}
