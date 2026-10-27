// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/PlayerAudioComponent.h"
#include "Cards/BaseCard.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundBase.h"

USoundBase* FPlayerSoundSet::Pick()
{
	// Skip empty entries so half-filled arrays are fine
	TArray<int32, TInlineAllocator<8>> Valid;
	for (int32 i = 0; i < Sounds.Num(); ++i)
	{
		if (Sounds[i])
		{
			Valid.Add(i);
		}
	}

	if (Valid.IsEmpty())
	{
		return nullptr;
	}

	// Avoid playing the same one twice in a row (when there's a choice)
	if (Valid.Num() > 1)
	{
		Valid.Remove(LastIndex);
	}

	LastIndex = Valid[FMath::RandRange(0, Valid.Num() - 1)];
	return Sounds[LastIndex];
}

UPlayerAudioComponent::UPlayerAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (UCardHandComponent* Hand = Owner->FindComponentByClass<UCardHandComponent>())
	{
		Hand->OnCardAdded.AddDynamic(this, &UPlayerAudioComponent::HandleCardAdded);
		Hand->OnCardUsed.AddDynamic(this, &UPlayerAudioComponent::HandleCardUsed);
		Hand->OnCardRefused.AddDynamic(this, &UPlayerAudioComponent::HandleCardRefused);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void UPlayerAudioComponent::Play2D(USoundBase* Sound, float Volume)
{
	if (!Sound)
	{
		return;
	}

	const float Pitch = 1.f + FMath::FRandRange(-PitchVariation, PitchVariation);

	// Master/SFX volume comes from the sound-class mix (UBaseGameInstance), so no volume scale here
	UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Sound, Volume, Pitch);
	if (!Audio)
	{
		return;
	}
	if (SFXSoundClass)
	{
		Audio->SoundClassOverride = SFXSoundClass;
	}
	Audio->Play();
}

void UPlayerAudioComponent::PlaySet(FPlayerSoundSet& Set)
{
	Play2D(Set.Pick(), Set.Volume);
}

// ---------------------------------------------------------------------------
// Card events
// ---------------------------------------------------------------------------
void UPlayerAudioComponent::HandleCardAdded(int32 SlotIndex, UBaseCard* Card)
{
	if (Card)
	{
		Play2D(Card->GetPickupSound());
	}
}

void UPlayerAudioComponent::HandleCardUsed(int32 SlotIndex, UBaseCard* Card)
{
	if (Card)
	{
		Play2D(Card->GetUseSound());
	}
}

void UPlayerAudioComponent::HandleCardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason)
{
	switch (Reason)
	{
	case ECardFeedback::EmptySlot:
		PlaySet(CardEmptySlotSounds);
		break;

	case ECardFeedback::CannotUse:
		// A card with its own "nope" sound wins over the generic one
		if (Card && Card->GetRefusedSound())
		{
			Play2D(Card->GetRefusedSound());
		}
		else
		{
			PlaySet(CardCannotUseSounds);
		}
		break;

	case ECardFeedback::HandFull:
		PlaySet(CardHandFullSounds);
		break;
	}
}

// ---------------------------------------------------------------------------
// Simple one-liners
// ---------------------------------------------------------------------------
void UPlayerAudioComponent::PlayInteract() { PlaySet(InteractSounds); }
void UPlayerAudioComponent::PlayInteractDenied() { PlaySet(InteractDeniedSounds); }
void UPlayerAudioComponent::PlayHurt() { PlaySet(HurtSounds); }
void UPlayerAudioComponent::PlayDeath() { PlaySet(DeathSounds); }
void UPlayerAudioComponent::PlayWin() { PlaySet(WinSounds); }
void UPlayerAudioComponent::PlayEnemySpotted() { PlaySet(EnemySpottedSounds); }
void UPlayerAudioComponent::PlayFootstep() { PlaySet(FootstepSounds); }
void UPlayerAudioComponent::PlayJump() { PlaySet(JumpSounds); }
void UPlayerAudioComponent::PlayLand() { PlaySet(LandSounds); }