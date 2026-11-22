// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/BaseAgentAudioComponent.h"
#include "AudioDevice.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeModulator.h"

// Sets default values for this component's properties
UBaseAgentAudioComponent::UBaseAgentAudioComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UBaseAgentAudioComponent::BeginPlay()
{
	Super::BeginPlay();
    BuildFootstepSoundCue();
}

void UBaseAgentAudioComponent::BuildFootstepSoundCue()
{
    if (FootstepWaves.IsEmpty())
    {
        return;
    }

    // 1. Create the transient SoundCue object
    GeneratedFootstepCue = NewObject<USoundCue>(this);

    // 2. Create a Random node and a Modulator node
    USoundNodeRandom* RandomNode = GeneratedFootstepCue->ConstructSoundNode<USoundNodeRandom>();
    USoundNodeModulator* ModulatorNode = GeneratedFootstepCue->ConstructSoundNode<USoundNodeModulator>();

    // Optional: slight pitch & volume variation per step
    ModulatorNode->PitchMin = 0.92f;
    ModulatorNode->PitchMax = 1.08f;
    ModulatorNode->VolumeMin = 0.90f;
    ModulatorNode->VolumeMax = 1.00f;

    // 3. Create a WavePlayer node for each SoundWave and plug it into the Random node
    for (int32 i = 0; i < FootstepWaves.Num(); ++i)
    {
        if (USoundWave* Wave = FootstepWaves[i])
        {
            USoundNodeWavePlayer* WavePlayer = GeneratedFootstepCue->ConstructSoundNode<USoundNodeWavePlayer>();
            WavePlayer->SetSoundWave(Wave);
            WavePlayer->bLooping = false;

            RandomNode->InsertChildNode(i);
            RandomNode->ChildNodes[i] = WavePlayer;
        }
    }

    // 4. Wire Random -> Modulator -> SoundCue Output
    ModulatorNode->InsertChildNode(0);
    ModulatorNode->ChildNodes[0] = RandomNode;

    GeneratedFootstepCue->FirstNode = ModulatorNode;
    GeneratedFootstepCue->LinkGraphNodesFromSoundNodes();
    GeneratedFootstepCue->PostLoad();
}

void UBaseAgentAudioComponent::PlayFootstep()
{
    UWorld* World = GetWorld();
    if (World && GeneratedFootstepCue && GetOwner())
    {
        if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
        {
            AudioDevice->PlaySoundAtLocation(
                GeneratedFootstepCue, World, 1.0f, 1.0f, 0.0f,
                GetOwner()->GetActorLocation(), FRotator::ZeroRotator
            );
        }
    }
}
