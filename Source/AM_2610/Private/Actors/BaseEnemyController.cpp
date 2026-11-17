// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BaseEnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "Utility/AITargetStack.h"

void ABaseEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Creates the Blackboard, so this has to happen before the first write
	if (BTAsset)
	{
		RunBehaviorTree(BTAsset);
	}

	if (UWorld* World = GetWorld())
	{
		if (UAITargetStack* Stack = World->GetSubsystem<UAITargetStack>())
		{
			// Re-possess safety: never hold two bindings
			if (TargetChangedHandle.IsValid())
			{
				Stack->OnTargetChanged.Remove(TargetChangedHandle);
			}

			TargetChangedHandle = Stack->OnTargetChanged.AddUObject(this, &ABaseEnemyController::HandleTargetChanged);

			// Catch up with whatever is already on the stack
			HandleTargetChanged(Stack->GetCurrentTarget());
		}
	}
}

void ABaseEnemyController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		if (UAITargetStack* Stack = World->GetSubsystem<UAITargetStack>())
		{
			Stack->OnTargetChanged.Remove(TargetChangedHandle);
		}
	}
	TargetChangedHandle.Reset();

	Super::OnUnPossess();
}

void ABaseEnemyController::HandleTargetChanged(AActor* NewTarget)
{
	// Can fire during level teardown (a decoy's EndPlay removes itself), so bail if we're mid-shutdown
	if (!IsValid(this) || !IsValid(GetPawn()))
	{
		return;
	}

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		if (NewTarget)
		{
			BB->SetValueAsObject(PlayerKey, NewTarget);
		}
		else
		{
			BB->ClearValue(PlayerKey);
		}
	}

	// Aborts the in-flight MoveTo so the tree re-issues it against the new target
	StopMovement();
}
