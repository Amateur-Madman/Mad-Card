// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AgentControl.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "../AM_2610.h"

AAgentControl::AAgentControl()
{
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 900.0f;
	SightConfig->LoseSightRadius = 1100.0f;
	SightConfig->PeripheralVisionAngleDegrees = 45.0f;
	// At this stage there's no team/affiliation setup yet, so only Neutral
	// (the default affiliation for everything right now) should detect.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 300.0f;

	AIPerception->ConfigureSense(*SightConfig);
}
void AAgentControl::OnPossess(APawn* InPawn)
{
		Super::OnPossess(InPawn);

	// Copy the pawn's team onto this controller BEFORE perception registers the listener
	if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		SetGenericTeamId(PawnTeam->GetGenericTeamId());
	}

	if (AIPerception)
	{
		// Guard prevents a double-bind if this controller re-possesses
		if (!AIPerception->OnTargetPerceptionUpdated.IsAlreadyBound(this, &AAgentControl::HandlePerceptionUpdated))
		{
			AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AAgentControl::HandlePerceptionUpdated);
		}
		// Re-register with the correct team so affiliation filtering uses it
		AIPerception->RequestStimuliListenerUpdate();
	}

	if (!BTAsset)
	{
		UE_LOG(Game, Warning, TEXT("AAgentController::OnPossess - BTAsset is not set"));
		return;
	}

	RunBehaviorTree(BTAsset);
}

void AAgentControl::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		BlackboardComp->SetValueAsObject(PlayerKeyName, Actor);
	}
	else
	{
		BlackboardComp->ClearValue(PlayerKeyName);
	}
}