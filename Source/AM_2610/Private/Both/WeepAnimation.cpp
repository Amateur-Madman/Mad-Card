// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/WeepAnimation.h"
#include "KismetAnimationLibrary.h"
#include "../AM_2610.h"

void UWeepAnimation::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RandomMovement();
}

void UWeepAnimation::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
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

void UWeepAnimation::RandomMovement()
{
	if (Movements.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Movements array is empty!"));
		return;
	}

	int32 RandomIndex = FMath::RandRange(0, Movements.Num() - 1);

	if (!Movements[RandomIndex])
	{
		UE_LOG(LogTemp, Warning, TEXT("BlendSpace[%d] is null, cannot play blend space"), RandomIndex);
		return;
	}

	CurrentMovement = Movements[RandomIndex];
}

