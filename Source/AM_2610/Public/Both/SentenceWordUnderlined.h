// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharUnderlined.h"
#include "SentenceWordUnderlined.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API USentenceWordUnderlined : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetupAccordingToDiff();

	const TArray<FString> EasyArray = { TEXT("Suffer!"), TEXT("You Lose!"), TEXT("The Mad-Man!"), TEXT("Gotcha!") };

	const TArray<FString> HardArray = { TEXT("You'll never escape the maze!"), TEXT("I was crazy once."), TEXT("You thought I'd let you go?") };

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UWrapBox* WordOrPhrase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TSubclassOf<UUserWidget> WordCharUnderlinedClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TSubclassOf<UUserWidget> SpaceWidgetClass;

	bool CheckInput(TCHAR LetterTyped);

private:
	void Setup(const FString& bWordOrPhrase);
	void AddToLetters(TArray<struct FMadLetter> bMadLetters);

	TArray<struct FMadLetter> AllLetters;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UGuessBox* WrongGuessBox;
};