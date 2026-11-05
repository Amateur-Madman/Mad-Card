// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/WordCharUnderlined.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"

//Make blank spaces for word
void UWordCharUnderlined::InitializeWord(FString& NewWord)
{
	if (!Word || !CharUnderlinedClass) return;

	Word->ClearChildren();

	TArray<TCHAR> CharArray = NewWord.GetCharArray();

    for (int32 i = 0; i < CharArray.Num()-1; i++)
    {
        UCharUnderlined* CharWidget = CreateWidget<UCharUnderlined>(this, CharUnderlinedClass);

        if (CharWidget)
        {
            if (!CheckIfNotLetter(CharWidget, CharArray[i]))
            {
                CharWidget->InitializeSpace(CharArray[i]);
                LetterDatas.Add(CharWidget->LetterData);
            }
            Word->AddChildToHorizontalBox(CharWidget);
        }
    }
}

void UWordCharUnderlined::AddIncorrect(TCHAR& bLetter)
{
    UCharUnderlined* CharWidget = CreateWidget<UCharUnderlined>(this, CharUnderlinedClass);
    if (CharWidget)
    {
        CharWidget->Incorrect(bLetter);
        LetterDatas.Add(CharWidget->LetterData);
        Word->AddChildToHorizontalBox(CharWidget);
    }

    UUserWidget* SpacerWidget = CreateWidget<UUserWidget>(GetWorld(), SpaceWidgetClass);
    if (SpacerWidget)
    {
        Word->AddChildToHorizontalBox(SpacerWidget);
    }
}

//Not having the player guess for punctuation
bool UWordCharUnderlined::CheckIfNotLetter(UCharUnderlined* bCharWidget, TCHAR& bLetter)
{
    for (int32 i = 0; i < Punctuation.Num(); i++)
    {
        if (bLetter == Punctuation[i])
        {
            bCharWidget->NotALetter(bLetter);
            return true;
        }
    }
    return false;
}

bool UWordCharUnderlined::CheckExistingLetters(TCHAR& bLetter)
{
    for(int32 i = 0; i < LetterDatas.Num(); i++)
    {
        if (bLetter == LetterDatas[i].Char)
        {
            return true;
        }
	}
    AddIncorrect(bLetter);
    return false;
}