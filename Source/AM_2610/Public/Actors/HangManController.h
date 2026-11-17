// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/WeepingAngelController.h"
#include "HangManController.generated.h"

/**
 * HangMan. Inherits from the Weeping Angel controller, which inherits from ABaseEnemyController.
 */
UCLASS()
class AM_2610_API AHangManController : public AWeepingAngelController
{
	GENERATED_BODY()
};
