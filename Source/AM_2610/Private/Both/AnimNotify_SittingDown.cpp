// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/AnimNotify_SittingDown.h"
#include "Actors/BasePlayer.h"
#include "Actors/ChairForHangman.h"

void UAnimNotify_SittingDown::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if(ABasePlayer* Player = Cast<ABasePlayer>(MeshComp->GetOwner()))
	{
		if(Player->ChairForHangman)
		{
			Player->ChairForHangman->OnPlayerReady.Broadcast();
		}
	}
}