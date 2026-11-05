// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ScannableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UScannableInterface : public UInterface
{
	GENERATED_BODY()
};

class AM_2610_API IScannableInterface
{
	GENERATED_BODY()

public:	
	//----------------------------------------------------------------------//
	// IScannableInterface 
	//----------------------------------------------------------------------//
	//----------------------------------------------------------------------//
	// Sent by the scan pulse when its wave reaches this actor.
	// Duration = how long the reveal should last.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Scan")
	void ScanRevealed(float Duration);
};
