// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/AnimNotify_OffCeiling.h"
#include "Both/HangAnimation.h"

void UAnimNotify_OffCeiling::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (UHangAnimation* AnimInstance = Cast<UHangAnimation>(MeshComp->GetAnimInstance()))
	{
		AnimInstance->OnFalling.Broadcast();
	}
}
