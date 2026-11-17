// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Agent.h"
#include "MadMan_Agent.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AMadMan_Agent : public AAgent
{
	GENERATED_BODY()
	
public:
	AMadMan_Agent();

protected:
	virtual void BeginPlay() override;

	class AWrittingBoard* WrittingBoard;

private:

};
