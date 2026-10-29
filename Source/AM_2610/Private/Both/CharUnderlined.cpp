// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/CharUnderlined.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCharUnderlined::SetLetter(FText bLetter)
{
	Letter->SetText(bLetter);
}
