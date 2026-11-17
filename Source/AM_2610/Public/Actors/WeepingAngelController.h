// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BaseEnemyController.h"
#include "WeepingAngelController.generated.h"

/**
 * Weeping Angel. BTAsset, PlayerKey, Behavior Tree startup and target-stack syncing all come
 * from ABaseEnemyController.
 */
UCLASS()
class AM_2610_API AWeepingAngelController : public ABaseEnemyController
{
	GENERATED_BODY()
};
