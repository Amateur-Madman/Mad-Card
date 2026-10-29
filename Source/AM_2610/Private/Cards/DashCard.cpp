// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/DashCard.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FName DashingTag(TEXT("Dashing"));

	struct FSavedResponse
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionChannel Channel;
		ECollisionResponse Original;
	};
}

bool UDashCard::CanUse_Implementation(AActor* User) const
{
	const ACharacter* Character = Cast<ACharacter>(User);
	if (!Character)
	{
		return false;
	}

	// Already dashing? Refuse (and keep the card), otherwise the second dash would record
	// the "ignore" responses as the originals and the player would never become solid again.
	return !Character->ActorHasTag(DashingTag);
}

void UDashCard::Use_Implementation(AActor* User)
{
	ACharacter* Character = Cast<ACharacter>(User);
	if (!Character)
	{
		return;
	}

	// Dash where the player is steering; fall back to where they're facing
	FVector Direction = Character->GetLastMovementInputVector();
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		Direction = Character->GetActorForwardVector();
	}

	Character->LaunchCharacter(Direction * DashSpeed, /*bXYOverride=*/true, /*bZOverride=*/false);

	// Ignore the phase channels on EVERY collidable component of the player (capsule, mesh...),
	// because an enemy sphere overlaps whichever one it touches. Remember the originals.
	TArray<FSavedResponse> Saved;
	TArray<UPrimitiveComponent*> Components;
	Character->GetComponents<UPrimitiveComponent>(Components);

	for (UPrimitiveComponent* Comp : Components)
	{
		if (Comp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		for (const TEnumAsByte<ECollisionChannel>& Channel : PhaseChannels)
		{
			Saved.Add({ Comp, Channel.GetValue(), Comp->GetCollisionResponseToChannel(Channel) });
			Comp->SetCollisionResponseToChannel(Channel, ECR_Ignore);
		}
	}

	Character->Tags.AddUnique(DashingTag);

	// The restore lives on the world's timer manager and captures only weak pointers - NOT this
	// card. A consumed card can be garbage collected before the timer fires, and the player must
	// never be left stuck in "ghost" mode.
	TWeakObjectPtr<ACharacter> WeakCharacter(Character);
	FTimerHandle RestoreHandle;
	Character->GetWorldTimerManager().SetTimer(
		RestoreHandle,
		FTimerDelegate::CreateLambda([WeakCharacter, Saved]()
			{
				// Changing a response re-runs overlap detection, so any enemy sphere the player is
				// still standing in gets a fresh BeginOverlap here.
				for (const FSavedResponse& Entry : Saved)
				{
					if (UPrimitiveComponent* Comp = Entry.Component.Get())
					{
						Comp->SetCollisionResponseToChannel(Entry.Channel, Entry.Original);
					}
				}

				if (ACharacter* Char = WeakCharacter.Get())
				{
					Char->Tags.Remove(DashingTag);
				}
			}),
		PhaseDuration,
		false);
}