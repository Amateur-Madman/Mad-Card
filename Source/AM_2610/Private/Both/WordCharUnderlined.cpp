// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/WordCharUnderlined.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
//#include "Both/CharUnderlined.h"

//Make blank spaces for word
void UWordCharUnderlined::InitializeWord(FString& NewWord)
{
	if (!Word || !CharUnderlinedClass) return;

	Word->ClearChildren();

	TArray<TCHAR> CharArray = NewWord.GetCharArray();

    for (int32 i = 0; i < CharArray.Num() - 1; i++)
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

//Not having the player guess for punctuation
bool UWordCharUnderlined::CheckIfNotLetter(UCharUnderlined* bCharWidget, TCHAR& bLetter)
{
    for (int32 i = 0; i < Punctuation.Num() - 1; i++)
    {
        if (bLetter == Punctuation[i])
        {
            bCharWidget->NotALetter(bLetter);
            return true;
        }
    }
    return false;
}
