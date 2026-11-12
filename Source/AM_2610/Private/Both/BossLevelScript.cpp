// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/BossLevelScript.h"
#include "Actors/ChairForHangman.h"
#include "Actors/WrittingBoard.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "Actors/BasePlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Both/SentenceWordUnderlined.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"

void ABossLevelScript::BeginPlay()
{
	Super::BeginPlay();
	if (ChairForHangman && WrittingBoard)
	{
		WrittingBoard->OnRight.AddDynamic(this, &ABossLevelScript::OnCorrectInput);	
		WrittingBoard->OnWrong.AddDynamic(this, &ABossLevelScript::OnIncorrectInput);

		if (IntroSequence)
		{
			SequenceQueue.Add(IntroSequence);
			PlayNextSequence();
		}
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

void ABossLevelScript::RegisterBoard(TObjectPtr<AWrittingBoard> Board)
{
	WrittingBoard = Board;
	TryLinkActors();
}

void ABossLevelScript::RegisterChair(TObjectPtr<AChairForHangman> Chair)
{
	ChairForHangman = Chair;
	TryLinkActors();
}

// Sequence handling
void ABossLevelScript::PlayNextSequence()
{
	if(SequenceQueue.Num() == 0)
	{
		return;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bPauseAtEnd = true; // Keeps actors at their final positions

	if (SequenceActor)
	{
		SequenceActor->Destroy();
		SequenceActor = nullptr;
	}

	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		GetWorld(), 
		SequenceQueue[0], 
		PlaybackSettings, 
		SequenceActor);

	if(SequencePlayer)
	{
		SequencePlayer->OnFinished.AddDynamic(this, &ABossLevelScript::OnIntroSequenceFinished);
		IsSequencePlaying = true;
		SequencePlayer->Play();
	}
}

void ABossLevelScript::EnqueueSequence(ULevelSequence* SequenceToAdd)
{
}

void ABossLevelScript::OnIntroSequenceFinished()
{
	UE_LOG(LogTemp, Log, TEXT("Intro sequence finished."));
	IsSequencePlaying = false;
}