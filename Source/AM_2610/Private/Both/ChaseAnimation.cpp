// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/ChaseAnimation.h"
#include "KismetAnimationLibrary.h"

void UChaseAnimation::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	if (Pawn)
	{
		FVector Velocity = Pawn->GetVelocity();
		//isvalid
		Speed = Velocity.Size();
		Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Pawn->GetActorRotation());

	}
	else
	{
		//isnotvalid
		Speed = 0.f;
		Direction = 0.f;
	}
}
