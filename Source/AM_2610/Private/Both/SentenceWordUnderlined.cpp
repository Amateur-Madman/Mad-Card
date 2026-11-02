// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/SentenceWordUnderlined.h"
//#include "Both/CharUnderlined.h"
#include "Both/WordCharUnderlined.h"
#include "Components/WrapBox.h"
#include "Enviornment/MazeSaveSubsystem.h"

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
            }
            else
            {
                RandomIndex = FMath::RandRange(0, EasyArray.Num() - 1);
                Setup(EasyArray[RandomIndex]);
            }
        }
    }

}

void USentenceWordUnderlined::Setup(const FString& bWordOrPhrase)
{
    if (!WordOrPhrase || !WordCharUnderlinedClass) return;

    WordOrPhrase->ClearChildren();

    TArray<FString> WordArray;
    bWordOrPhrase.ParseIntoArray(WordArray, TEXT(" "), true);

    for (int32 i = 0; i < WordArray.Num() - 1; i++)
    {
        //Update letter index throughout words
        UWordCharUnderlined* WordWidget = CreateWidget<UWordCharUnderlined>(this, WordCharUnderlinedClass);

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
    for (int32 i = 0; i < bMadLetters.Num() - 1; i++)
    {
        AllLetters.Add(bMadLetters[i]);
    }
}

FReply USentenceWordUnderlined::NativeOnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharEvent)
{
    CheckInput(InCharEvent.GetCharacter());
    return FReply::Handled();
}

bool USentenceWordUnderlined::CheckInput(TCHAR LetterTyped)
{
    for (int32 i = 0; i < AllLetters.Num() - 1; i++)
    {
        if (LetterTyped == AllLetters[i].Char)
        {
            AllLetters[i].GetParent()->FillLetter();
        }
    }
    return false;
}