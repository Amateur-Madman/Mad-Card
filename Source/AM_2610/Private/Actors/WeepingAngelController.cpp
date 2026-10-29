// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WeepingAngelController.h"

void AWeepingAngelController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (BTAsset)
	{
		RunBehaviorTree(BTAsset);
	}
}
