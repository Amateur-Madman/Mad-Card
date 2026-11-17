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

	bool bLoopSpline = true;
};
