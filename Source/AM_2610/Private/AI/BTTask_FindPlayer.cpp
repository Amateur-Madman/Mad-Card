// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_FindPlayer.h"
#include "AIController.h"
#include "Utility/AITargetStack.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

UBTTask_FindPlayer::UBTTask_FindPlayer()
{
	NodeName = TEXT("Find Player");

	PlayerKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindPlayer, PlayerKey), AActor::StaticClass());
}

void UBTTask_FindPlayer::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		PlayerKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_FindPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	if(UWorld* World = GetWorld())
	{
		TargetStack = World->GetSubsystem<UAITargetStack>();
		if (TargetStack)
		{
			BlackboardComp->SetValueAsObject(PlayerKey.SelectedKeyName, TargetStack->GetCurrentTarget());
		}
	}

	return EBTNodeResult::Type();
}