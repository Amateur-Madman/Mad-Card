// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/BossLevelScript.h"
#include "Actors/ChairForHangman.h"
#include "Actors/WrittingBoard.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "Actors/BasePlayerController.h"
#include "Actors/MadMan_Agent.h"
#include "Actors/MadManController.h"
#include "Both/MadManAnimation.h"
#include "GameFramework/GameStateBase.h"
#include "Both/SentenceWordUnderlined.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"

void ABossLevelScript::BeginPlay()
{
	Super::BeginPlay();

	PlayNextSequence(IntroSequence);

	if (ChairForHangman && WrittingBoard)
	{
		WrittingBoard->OnRight.AddDynamic(this, &ABossLevelScript::OnCorrectInput);	
		WrittingBoard->OnWrong.AddDynamic(this, &ABossLevelScript::OnIncorrectInput);
		WrittingBoard->OnCanProcess.AddDynamic(this, &ABossLevelScript::EndSequenceNow);
	}

	if(MadMan)
	{
		Player = Cast<ABasePlayer>(GetWorld()->GetFirstPlayerController()->GetPawn());
		MadMan->OnIntroSpot.AddDynamic(this, &ABossLevelScript::BossFocusPlayer);
		if (UMadManAnimation* AnimInstance = Cast<UMadManAnimation>(MadMan->GetMesh()->GetAnimInstance()))
		{
			AnimInstance->OnPointOver.AddDynamic(this, &ABossLevelScript::PresentHangman);
		}
	}
}


void ABossLevelScript::OnCorrectInput()
{
	int32 RandIndex = 0;
	if(RightSequences.Num() > 0)
	{
		RandIndex = FMath::RandRange(0, RightSequences.Num() - 1);
		PlayNextSequence(RightSequences[RandIndex]);
	}
}

void ABossLevelScript::OnIncorrectInput()
{
	int32 RandIndex = 0;
	if(WrittingBoard)
	{
		int32 Count = WrittingBoard->MainTextWidget->WrongGuessCount;
		int32 Limit = WrittingBoard->MainTextWidget->WrongGuessLimit;
		int32 Remaining = Limit - Count;
		if (Remaining <= 3)
		{
			if (Remaining == 3)
			{

			}
			Last3WrongInputs();
		}
		else
		{
			if(WrongSequences.Num() > 0)
			{
				RandIndex = FMath::RandRange(0, WrongSequences.Num() - 1);
				PlayNextSequence(WrongSequences[RandIndex]);
			}
		}
	}
}

void ABossLevelScript::Last3WrongInputs()
{
	if (WrongSequences.Num() > 0)
	{
		for (int32 i = 0; i < WrongSequences.Num(); i++)
		{

		}
	}
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
void ABossLevelScript::PlayNextSequence(class ULevelSequence* SequenceToPlay, bool BindSequence)
{
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bPauseAtEnd = true; // Keeps actors at their final positions

	if (SequenceActor)
	{
		SequenceActor->Destroy();
		SequenceActor = nullptr;
	}

	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		GetWorld(), 
		SequenceToPlay, 
		PlaybackSettings, 
		SequenceActor);

	if(SequencePlayer && BindSequence)
	{
		SequencePlayer->OnFinished.AddDynamic(this, &ABossLevelScript::OnSequenceFinished);
		SequencePlayer->Play();
	}
}

void ABossLevelScript::OnSequenceFinished()
{
	BossFocusPlayer();
}

void ABossLevelScript::PresentHangman()
{
	EditBossFocus(WrittingBoard);
	PlayNextSequence(PresentHangmanSequence, true);
}

void ABossLevelScript::EndSequenceNow()
{
	if (SequencePlayer && SequencePlayer->IsPlaying())
	{
		SequencePlayer->Stop();
	}
}

void ABossLevelScript::EditBossFocus(AActor* Target)
{
	MadMan->SetFoucsForRotation(Target);
}

void ABossLevelScript::BossFocusPlayer()
{
	EditBossFocus(Player);
}