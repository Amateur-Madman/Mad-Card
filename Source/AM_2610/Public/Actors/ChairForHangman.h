// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/BasePickup.h"
#include "ChairForHangman.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerReady);

/**
 * 
 */
UCLASS()
class AM_2610_API AChairForHangman : public ABasePickup
{
	GENERATED_BODY()
	
public:
	AChairForHangman(); 

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerReady OnPlayerReady;

	void LinkToWrittingBoard(class AWrittingBoard* InWrittingBoard) { WrittingBoard = InWrittingBoard; }

protected:
	virtual void HandlePickup(AActor* OtherActor, const FHitResult& SweepResult) override;
	virtual void BeginPlay() override;
	virtual void PostPickup() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UChildActorComponent* ChildActorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class ABasePlayer* CaughtPlayer;

	class AWrittingBoard* WrittingBoard;
private:
	void FindWrittingBoard();
};
