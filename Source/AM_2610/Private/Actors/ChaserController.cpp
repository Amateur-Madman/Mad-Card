// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChaserController.h"
#include <Perception/AIPerceptionComponent.h>
#include "BehaviorTree/BlackboardComponent.h"
#include <Perception/AISenseConfig_Sight.h>
#include "Actors/BasePlayer.h"

void AChaserController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (BTAsset)
	{
		RunBehaviorTree(BTAsset);
	}
}

AChaserController::AChaserController()
{
	NativePerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPeception"));
	if (NativePerception)
	{
		UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->SightRadius = 1500.0f;
		SightConfig->LoseSightRadius = 3000.0f;
		SightConfig->PeripheralVisionAngleDegrees = 45.0f;
		SightConfig->AutoSuccessRangeFromLastSeenLocation = 200.0f;
		NativePerception->ConfigureSense(*SightConfig);
	
		NativePerception->OnTargetPerceptionUpdated.AddDynamic(this, &AChaserController::HandlePerception);
	}

	PlayerKey = TEXT("Player");
}

void AChaserController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.WasSuccessfullySensed())
	{
		if(ABasePlayer* Player = Cast<ABasePlayer>(Actor))
		GetBlackboardComponent()->SetValueAsObject(PlayerKey, Actor);
	}
	else
	{
		GetBlackboardComponent()->ClearValue(PlayerKey);
	}
}
