// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SentenceWordUnderlined.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API USentenceWordUnderlined : public UUserWidget
{
	GENERATED_BODY()
	
public:
	TArray<FString> EasyArray = { TEXT("Suffer"), TEXT("You Lose"), TEXT("The Mad-Man"), TEXT("Gotcha") };

	TArray<FString> HardArray = { TEXT("You'll never escape the maze"), TEXT("I was crazy once"), TEXT("You thought I'd let you go") };

	UPROPERTY(meta = (BindWidget))
	class UWrapBox* WordOrPhrase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TSubclassOf<UUserWidget> WordCharUnderlinedClass;

protected:
	// This is the widget equivalent of BeginPlay()
	virtual void NativeOnInitialized() override;
};
