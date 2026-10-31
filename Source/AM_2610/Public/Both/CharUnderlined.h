// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/UserDefinedStruct.h" 
#include "CharUnderlined.generated.h"

/**
 * 
 */
//Custom Letter Struct
USTRUCT(BlueprintType)
struct FMadLetter
{
	GENERATED_BODY()

	//Placement in word or phrase
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	int32 Index;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FSlateFontInfo> FontOptions;
};

UCLASS()
class AM_2610_API UCharUnderlined : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Letter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Underscore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letter Setup")
	FMadLetter LetterData;

public:
	void SetLetter(FString bLetter);
	void InitializeSpace(int32 bIndex);
};
