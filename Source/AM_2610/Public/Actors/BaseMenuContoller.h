// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BaseMenuContoller.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ABaseMenuContoller : public APlayerController
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;

	// Soft reference - only resolved/loaded when actually needed, not at construction time
	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSoftClassPtr<class UUserWidget> MenuClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu")
	class UUserWidget* MenuObject;
};
