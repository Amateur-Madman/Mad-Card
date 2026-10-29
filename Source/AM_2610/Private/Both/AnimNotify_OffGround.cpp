// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/AnimNotify_OffGround.h"
#include "Both/HangAnimation.h"

void UAnimNotify_OffGround::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (UHangAnimation* AnimInstance = Cast<UHangAnimation>(MeshComp->GetAnimInstance()))
	{
		AnimInstance->OnFlying.Broadcast();
	}
}
