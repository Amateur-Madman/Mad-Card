// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/AnimNotify_LetGo.h"
#include "Actors/HangMan_Agent.h"
#include "Actors/HangManController.h"
#include "BrainComponent.h"

void UAnimNotify_LetGo::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (AHangMan_Agent* HangMan = Cast<AHangMan_Agent>(MeshComp->GetOwner()))
	{
		HangMan->OnAttackEnded.Broadcast();
	}

	if (APawn* AttackingPawn = Cast<APawn>(MeshComp->GetOwner()))
	{
		if (AHangManController* AIC = Cast<AHangManController>(AttackingPawn->GetController()))
		{
			if (UBrainComponent* BrainComp = AIC->GetBrainComponent())
			{
				FAIMessage::Send(AIC, FAIMessage(FName("Action Finished"), this, true));
			}
		}
	}
}