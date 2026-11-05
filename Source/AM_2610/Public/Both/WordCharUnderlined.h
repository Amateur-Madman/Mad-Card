// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharUnderlined.h"
#include "WordCharUnderlined.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UWordCharUnderlined : public UUserWidget
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = Default)
    void InitializeWord(FString& NewWord);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    class UHorizontalBox* Word;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
    TSubclassOf<UUserWidget> CharUnderlinedClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
    TSubclassOf<UUserWidget> SpaceWidgetClass;

    TArray<struct FMadLetter> LetterDatas;

    void AddIncorrect(TCHAR& bLetter);
    bool CheckExistingLetters(TCHAR& bLetter);

private:
    const TArray<TCHAR> Punctuation = {TEXT('\''), TEXT('-'), TEXT('?'), TEXT('!'), TEXT('.')};

    bool CheckIfNotLetter(class UCharUnderlined* bCharWidget, TCHAR& bLetter);
};
