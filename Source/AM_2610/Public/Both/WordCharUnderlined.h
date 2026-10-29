// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
    void InitializeWord(FString NewWord);

    UPROPERTY(meta = (BindWidget))
    class UHorizontalBox* Word;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
    TSubclassOf<UUserWidget> CharUnderlinedClass;
};
