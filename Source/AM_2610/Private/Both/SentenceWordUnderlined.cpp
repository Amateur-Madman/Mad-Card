// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/SentenceWordUnderlined.h"
#include "Both/CharUnderlined.h"
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

void USentenceWordUnderlined::Setup(FString& bWordOrPhrase)
{
    if (!WordOrPhrase || !WordCharUnderlinedClass) return;

    WordOrPhrase->ClearChildren();

    TArray<FString> WordArray;
    bWordOrPhrase.ParseIntoArray(WordArray, TEXT(" "), true);

    for (int32 WordIndex = 0; WordIndex < WordArray.Num(); WordIndex++)
    {
        UWordCharUnderlined* WordWidget = CreateWidget<UWordCharUnderlined>(this, WordCharUnderlinedClass);
        if (WordWidget)
        {
            FString CurrentWord = WordArray[WordIndex];
            WordWidget->InitializeWord(CurrentWord);
            WordOrPhrase->AddChildToWrapBox(WordWidget);
        }

        if (WordIndex < WordArray.Num() - 1)
        {
            UUserWidget* SpacerWidget = CreateWidget<UUserWidget>(GetWorld(), SpaceWidgetClass);
            WordOrPhrase->AddChildToWrapBox(SpacerWidget);
        }
    }

    this->ForceLayoutPrepass();
}