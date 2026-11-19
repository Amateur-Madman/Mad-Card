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
#include "../AM_2610.h"

void ABossLevelScript::BeginPlay()
{
	Super::BeginPlay();

	PlayNextSequence(IntroSequence, true);

	RightSequences.Append(MadManRightSequences);
	RightSequences.Append(LightRightSequences);
	WrongSequences.Append(MadManWrongSequences);
	WrongSequences.Append(LightWrongSequences);
	WrongSequences.Append(WorldWrongSequences);

	if (ChairForHangman && WrittingBoard)
	{
		WrittingBoard->OnRight.AddDynamic(this, &ABossLevelScript::OnCorrectInput);	
		WrittingBoard->OnWrong.AddDynamic(this, &ABossLevelScript::OnIncorrectInput);
		WrittingBoard->OnCanProcess.AddDynamic(this, &ABossLevelScript::EndSequenceNow);
	}

	if(MadMan)
	{
		Player = Cast<ABasePlayer>(GetWorld()->GetFirstPlayerController()->GetPawn());
		if (UMadManAnimation* AnimInstance = Cast<UMadManAnimation>(MadMan->GetMesh()->GetAnimInstance()))
		{
			AnimInstance->OnPointOver.AddDynamic(this, &ABossLevelScript::PresentHangman);
		}
	}
}

void ABossLevelScript::TryLinkActors()
{
	if (ChairForHangman && WrittingBoard)
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


void ABossLevelScript::OnCorrectInput()
{
	int32 RandIndex = 0;

	if(RightSequences.Num() > 0)
	{
		RandIndex = FMath::RandRange(0, RightSequences.Num() - 1);
		PlayNextSequence(RightSequences[RandIndex], true);
	}
}

void ABossLevelScript::OnIncorrectInput()
{
	int32 RandIndex = 0;
	int32 Count = WrittingBoard->MainTextWidget->WrongGuessCount;
	int32 Limit = WrittingBoard->MainTextWidget->WrongGuessLimit;
	int32 Remaining = Limit - Count;

	if (Remaining <= 2)
	{
		if (Remaining == 2)
		{

		}
		Last3WrongInputs();
	}
	else
	{
		if (WrongSequences.Num() > 0)
		{
			RandIndex = FMath::RandRange(0, WrongSequences.Num() - 1);
			PlayNextSequence(WrongSequences[RandIndex], true);
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

// Sequence handling
void ABossLevelScript::PlayNextSequence(class ULevelSequence* SequenceToPlay, bool BindSequence)
{
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

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

	if(SequencePlayer)
	{
		if (BindSequence)
		{
			SequencePlayer->OnFinished.AddDynamic(this, &ABossLevelScript::OnSequenceFinished);
		}
		FocusBasedOnSequence(SequenceToPlay);
		SequencePlayer->Play();
	}
}

void ABossLevelScript::OnSequenceFinished()
{
	MadMan->SetFoucsForRotation(Player);
	Player->BeginViewLock(WrittingBoard);
}

void ABossLevelScript::PresentHangman()
{
	MadMan->SetFoucsForRotation(WrittingBoard);
	PlayNextSequence(PresentHangmanSequence, true);
}

void ABossLevelScript::EndSequenceNow()
{
	if (SequencePlayer && SequencePlayer->IsPlaying())
	{
		SequencePlayer->Stop();
	}
}

SequenceType ABossLevelScript::CheckSequenceKind(ULevelSequence* Sequence)
{
	if (CheckSequenceContainer(Sequence, MadManRightSequences))
	{
		return SequenceType::BossRight;
	}

	if (CheckSequenceContainer(Sequence, MadManWrongSequences))
	{
		return SequenceType::BossWrong;
	}

	if (CheckSequenceContainer(Sequence, LightRightSequences))
	{
		return SequenceType::LightRight;
	}

	if (CheckSequenceContainer(Sequence, LightWrongSequences))
	{
		return SequenceType::LightWrong;
	}

	if (CheckSequenceContainer(Sequence, WorldWrongSequences))
	{
		return SequenceType::WorldWrong;
	}

	return SequenceType::None_OR_Empty;
}

bool ABossLevelScript::CheckSequenceContainer(ULevelSequence* Sequence, TArray<class ULevelSequence*>& Container)
{
	if (Container.Num() > 0)
	{
		for (int32 i = 0; i < Container.Num(); i++)
		{
			if (Sequence == Container[i])
			{
				return true;
			}
		}
	}
	return false;
}

void ABossLevelScript::RefocusPlayer(AActor* Target, FVector Offset)
{
	Player->BeginViewLock(Target, Offset);
}

void ABossLevelScript::FocusBasedOnSequence(ULevelSequence* Sequence)
{
	switch (CheckSequenceKind(Sequence))
	{
		case SequenceType::LightWrong:
			RefocusPlayer(WrittingBoard, FVector(0.0f, 90.0f, 0.0f));
			break;
		case SequenceType::LightRight:
			RefocusPlayer(WrittingBoard, FVector(0.0f, 90.0f, 0.0f));
			break;
		case SequenceType::BossWrong:
			RefocusPlayer(MadMan);
			break;
		case SequenceType::BossRight:
			RefocusPlayer(MadMan);
			break;
		case SequenceType::WorldWrong:
			RefocusPlayer(WrittingBoard, FVector(0.0f, -90.0f, 0.0f));
			break;
	}
}
