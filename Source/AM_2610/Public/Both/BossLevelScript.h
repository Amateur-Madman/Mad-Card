// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelScriptActor.h"
#include "BossLevelScript.generated.h"

/**
 * 
 */
enum class SequenceType
{
	None_OR_Empty,
	LightWrong,
	LightRight,
	WorldWrong,
	BossWrong,
	BossRight
};

UCLASS()
class AM_2610_API ABossLevelScript : public ALevelScriptActor
{
	GENERATED_BODY()
	
public:
	class ABasePlayer* Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	class AMadMan_Agent* MadMan;

	void RegisterBoard(TObjectPtr<class AWrittingBoard> Board);
	void RegisterChair(TObjectPtr<class AChairForHangman> Chair);

	UFUNCTION(BlueprintCallable, Category = "Sequencer")
	void PlayNextSequence(class ULevelSequence* SequenceToPlay, bool BindBoss = false, bool BindPlayer = false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> WrongSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> RightSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> MadManWrongSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> MadManRightSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> LightWrongSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> LightRightSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> WorldWrongSequences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences|Containers")
	TArray<class ULevelSequence*> SpawnSequences; 
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<class APointLight*> Lights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	class ULevelSequence* IntroSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences")
	class ULevelSequence* Last3Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Sequences")
	class ULevelSequence* PresentHangmanSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Objects")
	TObjectPtr<class AWrittingBoard> WrittingBoard;

protected:
	virtual void BeginPlay() override;

	//When Hangman is the final game
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangman|Objects")
	TObjectPtr<class AChairForHangman> ChairForHangman;

	//World Components for Input reactions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class ASpawner>> Spawners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class AActor>> FloorSections;

private:
	//Input tracking/ reaction
	UFUNCTION()
	void OnCorrectInput();

	UFUNCTION()
	void OnIncorrectInput();

	void Last3WrongInputs();

	void TryLinkActors();

	//Sequence Handling
	UPROPERTY()
	class ULevelSequencePlayer* SequencePlayer;

	UPROPERTY()
	class ALevelSequenceActor* SequenceActor;

	UFUNCTION()
	void OnSequenceFinishedBoss();

	UFUNCTION()
	void OnSequenceFinishedPlayer();

	UFUNCTION()
	void PresentHangman();

	UFUNCTION()
	void EndSequenceNow();

	SequenceType CheckSequenceKind(class ULevelSequence* Sequence);
	bool CheckSequenceContainer(class ULevelSequence* Sequence, TArray<class ULevelSequence*>& Container);

	//Character Handling
	void FocusBasedOnSequence(ULevelSequence* Sequence);
};
