// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MadMan_Agent.h"
#include "Both/BossLevelScript.h"
#include "Actors/ChairForHangman.h"
#include "Actors/WrittingBoard.h"

AMadMan_Agent::AMadMan_Agent()
{
}

void AMadMan_Agent::BeginPlay()
{
	Super::BeginPlay();

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
