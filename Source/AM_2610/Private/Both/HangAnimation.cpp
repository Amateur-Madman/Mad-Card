// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/HangAnimation.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Actors/Agent.h"
#include "../AM_2610.h"

void UHangAnimation::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OnFlying.AddDynamic(this, &UHangAnimation::LeaveGround);
	OnFalling.AddDynamic(this, &UHangAnimation::LeaveCeiling);
	OnHangAround.AddDynamic(this, &UHangAnimation::AnimHang);
	OnPlayerCaught.AddDynamic(this, &UHangAnimation::AnimAttack);
	OnNewLocation.AddDynamic(this, &UHangAnimation::AnimFly);
}

void UHangAnimation::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
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

void UHangAnimation::AnimFall()
{
	if(FallAsset)
	{
		PlaySlotAnimationAsDynamicMontage(FallAsset, ActionSlotName);
	}
}

void UHangAnimation::AnimFly()
{
	if (JumpAsset)
	{
		PlaySlotAnimationAsDynamicMontage(JumpAsset, ActionSlotName);
	}
}

void UHangAnimation::AnimHang()
{
	IsFalling = false;
	IsFlying = false;
}

void UHangAnimation::LeaveGround()
{
	IsFlying = true;
	IsAttacking = false;
}

void UHangAnimation::LeaveCeiling()
{
	IsFalling = true;
	IsAttacking = true;
}

void UHangAnimation::AnimAttack()
{
	if (AttackAsset)
	{
		PlaySlotAnimationAsDynamicMontage(AttackAsset, ActionSlotName);
	}
}