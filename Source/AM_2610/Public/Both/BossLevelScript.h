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
	TObjectPtr<class AMadMan> MadMan;

	void RegisterBoard(class AWrittingBoard* Board);
	void RegisterChair(class AChairForHangman* Chair);

protected:
	virtual void BeginPlay() override;

	//When Hangman is the final game
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<class AChairForHangman> ChairForHangman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<class AWrittingBoard> WrittingBoard;

	//World Components for Input reactions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class ASpawner>> Spawners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class AActor>> FloorSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<class APointLight>> Lights;

private:
	UFUNCTION()
	void OnCorrectInput();

	UFUNCTION()
	void OnIncorrectInput();

	void Last3WrongInputs();

	void TryLinkActors();
};
