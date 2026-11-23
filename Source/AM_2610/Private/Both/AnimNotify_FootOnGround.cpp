// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/AnimNotify_FootOnGround.h"
#include "Actors/Agent.h"

void UAnimNotify_FootOnGround::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (AAgent* Agent = Cast<AAgent>(MeshComp->GetOwner()))
	{
		Agent->OnFootstep.Broadcast();
	}
}