// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/MadManAnimation.h"
#include "KismetAnimationLibrary.h"

void UMadManAnimation::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	if (Pawn)
	{
		FVector Velocity = Pawn->GetVelocity();
		//Sequence Movement Velocity
		if (Velocity.IsNearlyZero() && Pawn->GetRootComponent())
		{
			Velocity = Pawn->GetRootComponent()->GetComponentVelocity();
		}
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
