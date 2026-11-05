// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GuessBox.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API UGuessBox : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UWordCharUnderlined* GuessBox;
};
