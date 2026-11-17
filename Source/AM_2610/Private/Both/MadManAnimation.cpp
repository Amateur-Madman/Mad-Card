// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/MadManAnimation.h"
#include "KismetAnimationLibrary.h"
#include "Actors/MadMan_Agent.h"
#include "Actors/MadManController.h"

void UMadManAnimation::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

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

void UMadManAnimation::AnimWave()
{
	if(WaveAsset)
	{
		int32 WaveCount = 4;
		PlaySlotAnimationAsDynamicMontage(WaveAsset, ActionSlotName, 0.25f, 0.25f, 1.0f, WaveCount);
		GetWorld()->GetTimerManager().SetTimer(AnimTimerHandle, this, &UMadManAnimation::AnimPoint, WaveAsset->GetPlayLength() * WaveCount, false);
	}
}

void UMadManAnimation::AnimPoint()
{
	if(PointAsset)
	{
		PlaySlotAnimationAsDynamicMontage(PointAsset, ActionSlotName);
		OnPointOver.Broadcast();
	}
}
