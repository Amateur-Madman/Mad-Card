// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()
public:	
	
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void TogglePause();
protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, Category = "Pause")
	TSubclassOf<class UPauseMenu> PauseMenuClass;

	UPROPERTY(Transient)
	class UPauseMenu* PauseMenuInstance = nullptr;
};
