// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BaseEnemyController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "ChaserController.generated.h"

/**
 * Chaser: stays idle until perception spots the player, then hunts whatever is on top of the
 * target stack (player or decoy). BTAsset / PlayerKey / OnPossess come from ABaseEnemyController.
 */
UCLASS()
class AM_2610_API AChaserController : public ABaseEnemyController
{
	GENERATED_BODY()

public:
	AChaserController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	UAIPerceptionComponent* NativePerception;

protected:
	// Only retargets a chaser that is already hunting
	virtual void HandleTargetChanged(AActor* NewTarget) override;

private:
	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);
};
