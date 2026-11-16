// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MadMan_Agent.h"
#include "Both/BossLevelScript.h"
#include "Components/SplineComponent.h"
#include "Actors/WrittingBoard.h"
#include "Actors/MadManController.h"

AMadMan_Agent::AMadMan_Agent()
{
}

void AMadMan_Agent::BeginPlay()
{
	Super::BeginPlay();

	if(AMadManController* MadManController = Cast<AMadManController>(GetController()))
	{
		if (IntroSpline)
		{
			if (USplineComponent* SplineComp = IntroSpline->FindComponentByClass<USplineComponent>())
			{
				MadManController->FollowSpline(SplineComp, false);
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ABossLevelScript* LevelScript = Cast<ABossLevelScript>(World->GetLevelScriptActor()))
		{
			WrittingBoard = LevelScript->WrittingBoard;
			if (WrittingBoard)
			{
				//WrittingBoard->OnRight.AddDynamic(this, &AMadMan_Agent::ScanRevealed_Implementation);
				//WrittingBoard->OnWrong.AddDynamic(this, &AMadMan_Agent::ScanRevealed_Implementation);
			}
		}
	}
}