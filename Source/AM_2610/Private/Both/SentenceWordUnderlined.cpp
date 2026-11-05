// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/SentenceWordUnderlined.h"
#include "Both/WordCharUnderlined.h"
#include "Components/WrapBox.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Both/GuessBox.h"

// Randomly choose from an array of strings based on the current difficulty level and set up the widget accordingly
void USentenceWordUnderlined::SetupAccordingToDiff()
{
    int32 RandomIndex;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        UMazeSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UMazeSaveSubsystem>();

        if (SaveSubsystem)
        {
            if (SaveSubsystem->GetDifficulty() == EMazeDifficulty::Hard)
            {
                RandomIndex = FMath::RandRange(0, HardArray.Num() - 1);
                Setup(HardArray[RandomIndex]);
				WrongGuessLimit = HardWrongGuessLimit;
            }
            else
            {
                RandomIndex = FMath::RandRange(0, EasyArray.Num() - 1);
                Setup(EasyArray[RandomIndex]);
				WrongGuessLimit = EasyWrongGuessLimit;
            }
        }
    }

}

void USentenceWordUnderlined::Setup(const FString& bWordOrPhrase)
{
    if (!WordOrPhrase || !WordCharUnderlinedClass) return;

    WordOrPhrase->ClearChildren();

	// Split the input string into words and create a UWordCharUnderlined widget for each word
    TArray<FString> WordArray;
    bWordOrPhrase.ParseIntoArray(WordArray, TEXT(" "), true);

    for (int32 i = 0; i < WordArray.Num(); i++)
    {
        UWordCharUnderlined* WordWidget = CreateWidget<UWordCharUnderlined>(this, WordCharUnderlinedClass);

		// Add the word widget to the wrap box and store its letter data
        if (WordWidget)
        {
            WordWidget->InitializeWord(WordArray[i]);
            AddToLetters(WordWidget->LetterDatas);
            WordOrPhrase->AddChildToWrapBox(WordWidget);
        }

        if (i < WordArray.Num() - 1)
        {
            UUserWidget* SpacerWidget = CreateWidget<UUserWidget>(GetWorld(), SpaceWidgetClass);
            WordOrPhrase->AddChildToWrapBox(SpacerWidget);
        }
    }

    this->ForceLayoutPrepass();
}

void USentenceWordUnderlined::AddToLetters(TArray<struct FMadLetter> bMadLetters)
{
    for (int32 i = 0; i < bMadLetters.Num(); i++)
    {
        AllLetters.Add(bMadLetters[i]);
    }
}

bool USentenceWordUnderlined::CheckIfAllLettersRevealed()
{
    for (int32 i = 0; i < AllLetters.Num(); i++)
    {
        if (AllLetters[i].GetParent()->IsRevealed == false)
        {
            return false;
        }
	}
	OnCompleted.Broadcast();
    return true;
}

bool USentenceWordUnderlined::CheckInput(TCHAR LetterTyped)
{
	ValidInput = false;

    for (int32 i = 0; i < AllLetters.Num(); i++)
    {
        if (LetterTyped == FChar::ToLower(AllLetters[i].Char) || LetterTyped == FChar::ToUpper(AllLetters[i].Char))
        {
            AllLetters[i].GetParent()->FillLetter();
            ValidInput = true;
        }
    }
    CheckIfAllLettersRevealed();

    if(ValidInput == false)
    {
        if(!WrongGuessBox->GuessBox->CheckExistingLetters(LetterTyped))
        {
            WrongGuessCount++;
            if(WrongGuessCount >= WrongGuessLimit)
            {
                // Handle reaching the wrong guess limit
				OnFailed.Broadcast();
            }
        }
    }
    return ValidInput;
}