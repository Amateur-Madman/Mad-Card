// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelScriptActor.h"
#include "BossLevelScript.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ABossLevelScript : public ALevelScriptActor
{
	GENERATED_BODY()
	
public:
	TObjectPtr<class ABasePlayer> Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<class AMadMan_Agent> MadMan;

	void RegisterBoard(TObjectPtr<class AWrittingBoard> Board);
	void RegisterChair(TObjectPtr<class AChairForHangman> Chair);

	UFUNCTION(BlueprintCallable, Category = "Sequencer")
	void EnqueueSequence(class ULevelSequence* SequenceToAdd);

	UFUNCTION(BlueprintCallable, Category = "Sequencer")
	void PlayNextSequence();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequencer")
	class ULevelSequence* IntroSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<class AWrittingBoard> WrittingBoard;

protected:
	virtual void BeginPlay() override;

	//When Hangman is the final game
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<class AChairForHangman> ChairForHangman;

	//World Components for Input reactions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class ASpawner>> Spawners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class AActor>> FloorSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class APointLight>> Lights;

private:
	//Input tracking/ reaction
	UFUNCTION()
	void OnCorrectInput();

	UFUNCTION()
	void OnIncorrectInput();

	void Last3WrongInputs();

	void TryLinkActors();

	//Sequence handling
	bool IsSequencePlaying = false;

	UPROPERTY()
	TArray<class ULevelSequence*> SequenceQueue;

	UPROPERTY()
	class ULevelSequencePlayer* SequencePlayer;

	UPROPERTY()
	class ALevelSequenceActor* SequenceActor;

	UFUNCTION()
	void OnIntroSequenceFinished();

	//UFUNCTION()
	//void OnSequenceFinished();
};
