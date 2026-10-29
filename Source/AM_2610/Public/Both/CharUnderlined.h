// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharUnderlined.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UCharUnderlined : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Letter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Underscore;

private:
	void SetLetter(FText bLetter);
};
