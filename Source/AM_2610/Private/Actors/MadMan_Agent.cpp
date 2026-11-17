// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MadMan_Agent.h"
#include "Both/BossLevelScript.h"
#include "Components/SplineComponent.h"
#include "Actors/WrittingBoard.h"
#include "Actors/MadManController.h"
#include "Both/MadManAnimation.h"
#include "GameFramework/CharacterMovementComponent.h"

AMadMan_Agent::AMadMan_Agent()
{
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void AMadMan_Agent::SetFoucsForRotation(AActor* Target)
{
	if (AMadManController* MadManController = Cast<AMadManController>(GetController()))
	{
		MadManController->SetFocus(Target);
	}
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
			OnIntroSpot.AddDynamic(this, &AMadMan_Agent::IntroduceGame);
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

void AMadMan_Agent::IntroduceGame()
{
	if(UMadManAnimation* AnimInstance = Cast<UMadManAnimation>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->AnimWave();
	}
}