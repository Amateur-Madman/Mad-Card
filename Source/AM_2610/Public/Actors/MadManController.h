// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MadManController.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API AMadManController : public AAIController
{
	GENERATED_BODY()
	
public:
	// Call this to start following a spline component
	UFUNCTION(BlueprintCallable, Category = "AI|Spline")
	void FollowSpline(class USplineComponent* SplineToFollow, bool bShouldLoop = true);

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
	UPROPERTY()
	TObjectPtr<class USplineComponent> TargetSpline;

	float CurrentDistance = 0.0f;

	// Smaller step = tighter adherence to curves (100-150 units is ideal)
	UPROPERTY(EditDefaultsOnly, Category = "AI|Spline")
	float StepDistance = 150.0f;

	// How close the AI gets before requesting the next point (prevents stopping/stuttering)
	UPROPERTY(EditDefaultsOnly, Category = "AI|Spline")
	float StepAcceptanceRadius = 60.0f;

	bool bLoopSpline = true;
	bool bIsFollowingSpline = false;
};
