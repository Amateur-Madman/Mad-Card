// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChaserController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Utility/AITargetStack.h"

AChaserController::AChaserController()
{
	// NOTE: reconstructed from the header. If your original constructor had different sight
	// numbers or extra senses, keep yours and only keep the OnTargetPerceptionUpdated binding.
	NativePerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("NativePerception"));
	SetPerceptionComponent(*NativePerception);

	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2000.f;
	SightConfig->LoseSightRadius = 2200.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	NativePerception->ConfigureSense(*SightConfig);
	NativePerception->SetDominantSense(SightConfig->GetSenseImplementation());
	NativePerception->OnTargetPerceptionUpdated.AddDynamic(this, &AChaserController::HandlePerception);
}

void AChaserController::HandleTargetChanged(AActor* NewTarget)
{
	UBlackboardComponent* BB = GetBlackboardComponent();

	// Not hunting yet: leave it alone and let perception start the chase.
	// Already hunting: swap the target (decoy on / decoy expired) and restart the move.
	if (BB && BB->GetValueAsObject(PlayerKey))
	{
		Super::HandleTargetChanged(NewTarget);
	}
}

void AChaserController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	UWorld* World = GetWorld();
	UAITargetStack* Stack = World ? World->GetSubsystem<UAITargetStack>() : nullptr;
	if (!BB || !Stack)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// Chase whatever the stack says, not necessarily the actor that was just seen
		BB->SetValueAsObject(PlayerKey, Stack->GetCurrentTarget());
	}
	else if (Actor == Stack->GetCurrentTarget())
	{
		// Lost sight of the thing we were chasing
		BB->ClearValue(PlayerKey);
	}
	// else: lost sight of the player while a decoy is on top, so keep chasing the decoy
}
