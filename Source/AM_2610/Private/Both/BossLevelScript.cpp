// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/BossLevelScript.h"
#include "Actors/ChairForHangman.h"
#include "Actors/WrittingBoard.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "Actors/BasePlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Both/SentenceWordUnderlined.h"

void ABossLevelScript::BeginPlay()
{
	Super::BeginPlay();
	if (ChairForHangman && WrittingBoard)
	{
		WrittingBoard->OnRight.AddDynamic(this, &ABossLevelScript::OnCorrectInput);	
		WrittingBoard->OnWrong.AddDynamic(this, &ABossLevelScript::OnIncorrectInput);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ChairForHangman or WrittingBoard is not set in BossLevelScript."));
	}
}


void ABossLevelScript::OnCorrectInput()
{
}

void ABossLevelScript::OnIncorrectInput()
{
	if(WrittingBoard)
	{
		int32 Count = WrittingBoard->MainTextWidget->WrongGuessCount;
		int32 Limit = WrittingBoard->MainTextWidget->WrongGuessLimit;
		if (Limit - Count <= 3)
		{
			Last3WrongInputs();
		}
		else
		{

		}
	}
}

void ABossLevelScript::Last3WrongInputs()
{
}

void ABossLevelScript::TryLinkActors()
{
	if(ChairForHangman && WrittingBoard)
	{
		WrittingBoard->LinkToChairForHangman(ChairForHangman);
		ChairForHangman->LinkToWrittingBoard(WrittingBoard);
	}
}

void ABossLevelScript::RegisterBoard(AWrittingBoard* Board)
{
	WrittingBoard = Board;
	TryLinkActors();
}

void ABossLevelScript::RegisterChair(AChairForHangman* Chair)
{
	ChairForHangman = Chair;
	TryLinkActors();
}
