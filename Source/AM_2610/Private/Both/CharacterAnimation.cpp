// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/CharacterAnimation.h"
#include "KismetAnimationLibrary.h"
#include "../AM_2610.h"

void UCharacterAnimation::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
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
		PreviewWindowUpdate();
		Speed = 0.f;
		Direction = 0.f;
	}
}

void UCharacterAnimation::PreviewWindowUpdate_Implementation()
{
	if (DebugFire)
	{
		FireAnimation();
		DebugFire = false;
	}

	if (DebugHit)
	{
		HitAnimation();
		DebugHit = false;
	}

	if (DebugDeath)
	{
		DeathAnimation();
		DebugDeath = false;
	}
	if (DebugReload)
	{
		ReloadAnimation();
		DebugReload = false;
	} 
}
void UCharacterAnimation::FireAnimation()
{
	PlaySlotAnimationAsDynamicMontage(FireAsset, SlotName, 0.25f, 0.25f, 1.f, 1);
}



void UCharacterAnimation::DeathAnimation_Implementation()
{
	if (DeathAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeathAnimation: DeathAssets is empty, cannot pick a death animation"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("DeathAnimation: Picking a random death animation from %d options"), DeathAssets.Num());
	int32 RandomIndex = FMath::RandRange(0, DeathAssets.Num() - 1);
	if (!DeathAssets[RandomIndex])
	{
		UE_LOG(LogTemp, Warning, TEXT("DeathAnimation: DeathAssets[%d] is null, cannot play death animation"), RandomIndex);
		return;
	}
	CurrentDeathAsset = DeathAssets[RandomIndex];

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DeathEndedTimerHandle, this, &UCharacterAnimation::HandleDeathEnded, CurrentDeathAsset->GetPlayLength(), false);
	}
}
void UCharacterAnimation::HandleDeathEnded()
{
	OnDeathEnded.Broadcast();
}
void UCharacterAnimation::HitAnimation()
{
	PlaySlotAnimationAsDynamicMontage(HitAsset, SlotName, 0.25f, 0.25f, 1.f, 1);
	UE_LOG(LogTemp, Warning, TEXT("Hit Animation Played"));
}
void UCharacterAnimation::ReloadAnimation()
{
	PlaySlotAnimationAsDynamicMontage(ReloadAsset, SlotName, 0.25f, 0.25f, 1.f, 1);
}