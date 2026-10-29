// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/WordCharUnderlined.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Both/CharUnderlined.h"

//Make blank spaces for word
void UWordCharUnderlined::InitializeWord(FString NewWord)
{
	if (!Word || !CharUnderlinedClass) return;

	Word->ClearChildren();

	TArray<TCHAR> CharArray = NewWord.GetCharArray();

    for (int32 i = 0; i < CharArray.Num() - 1; i++)
    {
        UCharUnderlined* CharWidget = CreateWidget<UCharUnderlined>(GetWorld(), CharUnderlinedClass);
        if (CharWidget)
        {
            //UTextBlock* LetterMeshText = Cast<UTextBlock>(CharWidget->GetWidgetFromName(TEXT("Letter")));
            //if (LetterMeshText)
            //{
            //    FString SingleLetterStr = FString(1, &CharArray[i]);
            //    LetterMeshText->SetText(FText::FromString(SingleLetterStr));
            //}

            Word->AddChildToHorizontalBox(CharWidget);
        }
    }
}
