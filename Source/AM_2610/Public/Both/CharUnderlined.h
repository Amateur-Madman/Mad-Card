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

	class UCharUnderlined* ParentClass = nullptr;

	class UCharUnderlined* GetParent() { return ParentClass; }

	TCHAR Char;

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

public:
	void FillLetter();
	void InitializeSpace(TCHAR& bLetter);
	void NotALetter(TCHAR& bLetter);
	void Incorrect(TCHAR& bLetter);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letter Setup")
	FMadLetter LetterData;

	bool IsRevealed = false;
};
