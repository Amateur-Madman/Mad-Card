// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Agent.h"
#include "HangMan_Agent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackEnded);
/**
 * 
 */
UCLASS()
class AM_2610_API AHangMan_Agent : public AAgent
{
	GENERATED_BODY()
public:
	AHangMan_Agent();

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnAttackEnded OnAttackEnded;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UCapsuleComponent* DetectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class ABasePlayer* CaughtPlayer;

	void BeginPlay() override;
	
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

private:
	UFUNCTION()
	void OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void Fall();

	UFUNCTION()
	void Fly();

	UFUNCTION()
	void ReleasePlayer();
};
