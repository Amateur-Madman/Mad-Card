// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/CharUnderlined.h"
#include "Components/TextBlock.h"

void UCharUnderlined::FillLetter()
{
	FString CharString = FString(1, &LetterData.Char);
	Letter->SetText(FText::FromString(CharString));
	IsRevealed = true;
}

//Auto fill spaces with a possible random font
void UCharUnderlined::InitializeSpace(TCHAR& bLetter)
{
	int32 RandomIndex = FMath::RandRange(0, LetterData.FontOptions.Num() - 1);
	if (LetterData.FontOptions.IsValidIndex(RandomIndex))
	{
		FSlateFontInfo RandFont = LetterData.FontOptions[RandomIndex];
		Letter->SetFont(RandFont);
	}
	LetterData.ParentClass = this;
	LetterData.Char = bLetter;
}

//Auto fill in punctuation and remove the underscore
void UCharUnderlined::NotALetter(TCHAR& bLetter)
{
	Letter->SetText(FText::FromString(FString(1, &bLetter)));
	Underscore->SetText(FText::FromString(""));
	IsRevealed = true;
}

void UCharUnderlined::Incorrect(TCHAR& bLetter)
{
	NotALetter(bLetter);
	Letter->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	LetterData.Char = bLetter;
}