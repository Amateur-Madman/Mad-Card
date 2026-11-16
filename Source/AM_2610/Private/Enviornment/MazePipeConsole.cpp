// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipeConsole.h"
#include "Enviornment/MazePuzzleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AMazePipeConsole::AMazePipeConsole()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Movable);

    ConsoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMesh"));
    ConsoleMesh->SetupAttachment(SceneRoot);
    ConsoleMesh->SetMobility(EComponentMobility::Movable);
    ConsoleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ConsoleMesh->SetCanEverAffectNavigation(false);

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(SceneRoot);
    InteractionBox->SetMobility(EComponentMobility::Movable);
    InteractionBox->InitBoxExtent(FVector(30.0f, 40.0f, 50.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionObjectType(ECC_GameTraceChannel1);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBox->SetGenerateOverlapEvents(true);
    InteractionBox->SetCanEverAffectNavigation(false);
    InteractionBox->SetHiddenInGame(true);
    InteractionBox->SetVisibility(false);

    SteamTestAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SteamTestAudio"));
    SteamTestAudio->SetupAttachment(SceneRoot);
    SteamTestAudio->bAutoActivate = false;
    SteamTestAudio->bStopWhenOwnerDestroyed = true;
}

UMazePuzzleComponent* AMazePipeConsole::GetPipePuzzle() const
{
    const auto FindPuzzle = [this](AActor* Room) -> UMazePuzzleComponent*
        {
            if (!IsValid(Room) || Room == this || Room->GetWorld() != GetWorld())
            {
                return nullptr;
            }
            UMazePuzzleComponent* Puzzle = Room->FindComponentByClass<UMazePuzzleComponent>();
            return IsValid(Puzzle) && Puzzle->bUsePipePuzzle ? Puzzle : nullptr;
        };
    if (PuzzleRoom.Get() != nullptr)
    {
        return FindPuzzle(PuzzleRoom.Get());
    }
    for (AActor* Room : { GetParentActor(), GetAttachParentActor(), GetOwner() })
    {
        if (UMazePuzzleComponent* Puzzle = FindPuzzle(Room))
        {
            return Puzzle;
        }
    }
    return nullptr;
}

bool AMazePipeConsole::CanInteract_Implementation(AActor* Interactor)
{
    APawn* Player = Cast<APawn>(Interactor);
    UMazePuzzleComponent* Puzzle = GetPipePuzzle();
    return GetWorld() && GetWorld()->IsGameWorld() && IsValid(Player)
        && Player->IsPlayerControlled() && Player->InputEnabled() && Player->GetWorld() == GetWorld()
        && IsValid(Puzzle) && Puzzle->CanActivateConsole(Player);
}

FInteractPrompt AMazePipeConsole::GetInteractPrompt_Implementation(AActor* Interactor)
{
    FInteractPrompt Prompt;
    if (CanInteract_Implementation(Interactor))
    {
        Prompt.Target = this;
        Prompt.Slot = InteractSlot;
        Prompt.Verb = InteractVerb;
    }
    return Prompt;
}

void AMazePipeConsole::Interact_Implementation(AActor* Interactor)
{
    if (CanInteract_Implementation(Interactor))
    {
        if (UMazePuzzleComponent* Puzzle = GetPipePuzzle())
        {
            if (Puzzle->ActivateConsole(Cast<APawn>(Interactor)) && Puzzle->bPipeSteamChecking && IsValid(SteamTestAudio))
            {
                SteamTestAudio->Play(0.0f);
            }
        }
    }
}