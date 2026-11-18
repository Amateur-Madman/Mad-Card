// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipeConsole.h"
#include "Enviornment/MazePuzzleComponent.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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
    InteractionBox->InitBoxExtent(FVector(4.0f, 18.0f, 24.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBox->SetGenerateOverlapEvents(false);
    InteractionBox->SetCanEverAffectNavigation(false);
    InteractionBox->SetHiddenInGame(true);
    InteractionBox->SetVisibility(false);

    InteractionRange = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRange"));
    InteractionRange->SetupAttachment(InteractionBox);
    InteractionRange->SetMobility(EComponentMobility::Movable);
    InteractionRange->SetAbsolute(false, false, true);
    InteractionRange->InitSphereRadius(MaxUseDistance);
    InteractionRange->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionRange->SetCollisionObjectType(ECC_GameTraceChannel1);
    InteractionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionRange->SetGenerateOverlapEvents(true);
    InteractionRange->SetCanEverAffectNavigation(false);
    InteractionRange->SetHiddenInGame(true);
    InteractionRange->SetVisibility(false);

    SteamTestAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SteamTestAudio"));
    SteamTestAudio->SetupAttachment(SceneRoot);
    SteamTestAudio->bAutoActivate = false;
    SteamTestAudio->bStopWhenOwnerDestroyed = true;
}

void AMazePipeConsole::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // The player's end-overlap removes the entire actor. Keep a single registration
    // shape even when an older Blueprint saved Pawn overlap on InteractionBox.
    InteractionBox->SetGenerateOverlapEvents(false);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    SetMaxUseDistance(MaxUseDistance);
    InteractionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AMazePipeConsole::SetMaxUseDistance(float NewDistance)
{
    MaxUseDistance = FMath::IsFinite(NewDistance) ? FMath::Max(0.0f, NewDistance) : 0.0f;
    if (IsValid(InteractionRange.Get()) && IsValid(InteractionBox.Get()))
    {
        const FVector Extent = InteractionBox->GetScaledBoxExtent();
        const float ScreenRadius = Extent.ContainsNaN() ? 0.0f : static_cast<float>(FVector(0.0, Extent.Y, Extent.Z).Size());
        InteractionRange->SetSphereRadius(MaxUseDistance + ScreenRadius);
    }
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
    FHitResult Obstruction;
    LastFocusCheck = EvaluateFocus(Cast<APawn>(Interactor), Obstruction);
    LastBlockingComponent = LastFocusCheck == EMazePipeConsoleFocus::ViewBlocked ? Obstruction.GetComponent() : nullptr;
    return LastFocusCheck == EMazePipeConsoleFocus::Ready;
}

EMazePipeConsoleFocus AMazePipeConsole::EvaluateFocus(APawn* Player, FHitResult& Obstruction) const
{
    const APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!IsValid(Controller) || !Player->InputEnabled())
    {
        return EMazePipeConsoleFocus::PlayerUnavailable;
    }
    const UMazePuzzleComponent* Puzzle = GetPipePuzzle();
    if (!IsValid(Puzzle))
    {
        return EMazePipeConsoleFocus::NoPipeRoom;
    }
    if (!Puzzle->CanActivateConsole(Player))
    {
        return EMazePipeConsoleFocus::PuzzleNotReady;
    }
    if (!IsValid(InteractionBox.Get()) || !FMath::IsFinite(MaxUseDistance) || MaxUseDistance <= 0.0f)
    {
        return EMazePipeConsoleFocus::InvalidTarget;
    }
    const FVector Extent = InteractionBox->GetScaledBoxExtent().GetAbs();
    if (Extent.ContainsNaN() || Extent.Y <= 0.0 || Extent.Z <= 0.0)
    {
        return EMazePipeConsoleFocus::InvalidTarget;
    }
    FVector Origin;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Origin, Rotation);
    const FVector Direction = Rotation.Vector();
    const FVector Center = InteractionBox->GetComponentLocation();
    const FVector Normal = InteractionBox->GetForwardVector();
    const double Incidence = FVector::DotProduct(Direction, Normal);
    if (FMath::Abs(Incidence) < 0.01)
    {
        return EMazePipeConsoleFocus::AimOffScreen;
    }

    // Intersect the glass at local X = 0. Entering the trigger's depth cannot
    // make a close, forward-facing press fail or a sideways press succeed.
    const double Distance = FVector::DotProduct(Center - Origin, Normal) / Incidence;
    if (!FMath::IsFinite(Distance) || Distance <= 0.0)
    {
        return EMazePipeConsoleFocus::AimOffScreen;
    }
    if (Distance > MaxUseDistance)
    {
        return EMazePipeConsoleFocus::OutOfReach;
    }
    const FVector ScreenPoint = Origin + Direction * Distance;
    const FVector Offset = ScreenPoint - Center;
    if (FMath::Abs(FVector::DotProduct(Offset, InteractionBox->GetRightVector())) > Extent.Y
        || FMath::Abs(FVector::DotProduct(Offset, InteractionBox->GetUpVector())) > Extent.Z)
    {
        return EMazePipeConsoleFocus::AimOffScreen;
    }

    FCollisionQueryParams Query(SCENE_QUERY_STAT(MazePipeConsoleFocus), true, Player);
    Query.AddIgnoredComponent(InteractionBox.Get());
    TArray<AActor*> AttachedActors;
    Player->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);
    if (!GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, ScreenPoint, ECC_Visibility, Query))
    {
        return EMazePipeConsoleFocus::Ready;
    }
    const AActor* HitActor = Obstruction.GetActor();
    if (HitActor == this)
    {
        return EMazePipeConsoleFocus::Ready;
    }
    // The visible monitor may be a separate mesh in the room. Allow a shallow
    // mounting offset at the target, while keeping nearer room geometry solid.
    constexpr double GlassToleranceCm = 4.0;
    const AActor* Room = Puzzle->GetOwner();
    if (Distance - Obstruction.Distance <= GlassToleranceCm && IsValid(HitActor) && IsValid(Room)
        && (HitActor == Room || HitActor->GetParentActor() == Room || HitActor->IsAttachedTo(Room)))
    {
        return EMazePipeConsoleFocus::Ready;
    }
    return EMazePipeConsoleFocus::ViewBlocked;
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
            if (Puzzle->ActivateConsole(Cast<APawn>(Interactor))
                && (Puzzle->bPipeSteamChecking || Puzzle->bPipeTrapTriggered) && IsValid(SteamTestAudio))
            {
                SteamTestAudio->Play(0.0f);
            }
        }
    }
}