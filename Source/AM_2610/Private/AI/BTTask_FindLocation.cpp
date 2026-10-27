// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_FindLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_FindLocation::UBTTask_FindLocation()
{
	NodeName = TEXT("Find Location");

	// Restrict the picker in the BT editor to Vector-type blackboard keys
	LocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindLocation, LocationKey));
}

void UBTTask_FindLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		LocationKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_FindLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(ControlledPawn->GetWorld());
	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	const FVector Origin = ControlledPawn->GetActorLocation();

	FNavLocation ResultLocation;
	const bool bFound = NavSystem->GetRandomPointInNavigableRadius(Origin, Radius, ResultLocation);

	if (bFound)
	{
		BlackboardComp->SetValueAsVector(LocationKey.SelectedKeyName, ResultLocation.Location);
	}

	return bFound ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}