// Fill out your copyright notice in the Description page of Project Settings.


#include "Enviornment/TrapRoomComponent.h"
#include "Actors/BaseGameMode.h"
#include "Actors/CardPickup.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Enviornment/MazeGenerator.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

UTrapRoomComponent::UTrapRoomComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrapRoomComponent::BeginPlay()
{
    Super::BeginPlay();

    // Let the room finish creating its child actors before finding their components.
    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTrapRoomComponent::InitializeTrap);
}

void UTrapRoomComponent::InitializeTrap()
{
    AActor* Room = GetOwner();

    if (!IsValid(Room))
    {
        return;
    }

    MazeGenerator = Cast<AMazeGenerator>(Room->GetOwner());

    TArray<UBoxComponent*> Boxes;
    Room->GetComponents<UBoxComponent>(Boxes);
    int32 RoomAreaCount = 0;
    int32 DamageBoxCount = 0;
    int32 DamageCapsuleCount = 0;

    for (UBoxComponent* Box : Boxes)
    {
        if (IsValid(Box) && !RoomAreaTag.IsNone() && Box->ComponentHasTag(RoomAreaTag))
        {
            RoomArea = Box;
            ++RoomAreaCount;
        }
    }

    // Child flamethrower Blueprints can use the same component tags as the room.
    TArray<UActorComponent*> Components;
    Room->GetComponents<UActorComponent>(Components, true);

    for (UActorComponent* Component : Components)
    {
        if (!IsValid(Component))
        {
            continue;
        }

        // Every tagged box or capsule counts, including components in child actors.
        if (Component != RoomArea.Get() && !DamageAreaTag.IsNone() && Component->ComponentHasTag(DamageAreaTag))
        {
            if (UBoxComponent* Box = Cast<UBoxComponent>(Component))
            {
                DamageAreas.Add(Box);
                ++DamageBoxCount;
            }
            else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
            {
                DamageAreas.Add(Capsule);
                ++DamageCapsuleCount;
            }
        }

        if (!FlameTag.IsNone() && Component->ComponentHasTag(FlameTag))
        {
            Component->SetAutoActivate(false);
            FlameComponents.Add(Component);
        }

        ULightComponent* Light = Cast<ULightComponent>(Component);

        if (Light && !WarningLightTag.IsNone() && Light->ComponentHasTag(WarningLightTag))
        {
            WarningLights.Add(Light);
        }

        UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);

        if (Mesh && !WarningMeshTag.IsNone() && Mesh->ComponentHasTag(WarningMeshTag))
        {
            for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
            {
                UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(Index);

                if (Material)
                {
                    WarningMaterials.Add(Material);
                }
            }
        }
    }

    SetWarningLights(false);
    SetFlames(false);

    if (RoomAreaCount != 1 || DamageAreas.IsEmpty() || FlameComponents.IsEmpty() || WarningLights.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Trap: %s needs one RoomArea box, at least one separate TrapDamageArea box or capsule, at least one TrapFlame Niagara component, and at least one TrapWarningLight light. Found room boxes=%d, damage boxes=%d, damage capsules=%d, flames=%d, lights=%d. Trap disabled."), *Room->GetName(), RoomAreaCount, DamageBoxCount, DamageCapsuleCount, FlameComponents.Num(), WarningLights.Num());
        return;
    }

    bTrapReady = true;

    // One position check avoids capsule/interaction-sphere double overlaps.
    GetWorld()->GetTimerManager().SetTimer(CheckTimer, this, &UTrapRoomComponent::CheckPlayer, 0.1f, true);
    CheckPlayer();
}

bool UTrapRoomComponent::CanRunTrap() const
{
    if (!bTrapReady || !RoomArea.IsValid() || DamageAreas.IsEmpty() || !IsValid(GetOwner()))
    {
        return false;
    }

    // Candidate rooms stay harmless while the generator is testing layouts.
    return !MazeGenerator.IsValid() || MazeGenerator->LoopState == EMazeLoopState::TraversingMaze;
}

bool UTrapRoomComponent::IsPlayerInsideRoom(APawn* Player) const
{
    return IsPlayerInsideBox(Player, RoomArea.Get());
}

bool UTrapRoomComponent::IsPlayerInsideDamageArea(APawn* Player) const
{
    // Overlapping areas still count as one hit through the existing loss check.
    for (const TWeakObjectPtr<UShapeComponent>& Area : DamageAreas)
    {
        if (!Area.IsValid())
        {
            continue;
        }

        if (const UBoxComponent* Box = Cast<UBoxComponent>(Area.Get()))
        {
            if (IsPlayerInsideBox(Player, Box))
            {
                return true;
            }
        }
        else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Area.Get()))
        {
            if (IsPlayerInsideCapsule(Player, Capsule))
            {
                return true;
            }
        }
    }

    return false;
}

bool UTrapRoomComponent::IsPlayerInsideBox(APawn* Player, const UBoxComponent* Box) const
{
    if (!IsValid(Player) || !Player->IsPlayerControlled() || !IsValid(Box))
    {
        return false;
    }

    // Local space also handles rooms that rotate or use a different scale.
    const FVector Position = Box->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
    const FVector Extent = Box->GetUnscaledBoxExtent();

    return FMath::Abs(Position.X) <= Extent.X && FMath::Abs(Position.Y) <= Extent.Y && FMath::Abs(Position.Z) <= Extent.Z;
}

bool UTrapRoomComponent::IsPlayerInsideCapsule(APawn* Player, const UCapsuleComponent* Capsule) const
{
    if (!IsValid(Player) || !Player->IsPlayerControlled() || !IsValid(Capsule))
    {
        return false;
    }

    // Use the rounded ends and scaled size instead of the capsule's bounding box.
    const FVector Position = Capsule->GetComponentTransform().InverseTransformPositionNoScale(Player->GetActorLocation());
    const double Radius = Capsule->GetScaledCapsuleRadius();
    const double SegmentHalfHeight = FMath::Max(0.0, Capsule->GetScaledCapsuleHalfHeight() - Radius);
    const double ClosestZ = FMath::Clamp(Position.Z, -SegmentHalfHeight, SegmentHalfHeight);

    return FMath::Square(Position.X) + FMath::Square(Position.Y) + FMath::Square(Position.Z - ClosestZ) <= FMath::Square(Radius);
}

void UTrapRoomComponent::CheckPlayer()
{
    if (!CanRunTrap())
    {
        if (TrapState == ETrapRoomState::Warning || TrapState == ETrapRoomState::Firing)
        {
            ResetTrap();
        }

        return;
    }

    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bInside = IsPlayerInsideRoom(Player);

    if (TrapState == ETrapRoomState::Idle && bInside)
    {
        StartWarning();
    }
    else if (TrapState == ETrapRoomState::Firing && bInside && IsPlayerInsideDamageArea(Player) && !bLossReported)
    {
        ABaseGameMode* GameMode = Cast<ABaseGameMode>(UGameplayStatics::GetGameMode(this));

        if (GameMode)
        {
            bLossReported = true;

            // Use the same loss path as enemies, including the existing retry/save rules.
            GameMode->TriggerLose();
        }
        else
        {
            bLossReported = true;
            UE_LOG(LogTemp, Error, TEXT("Trap: %s could not find BaseGameMode. Check the map's GameMode Override."), *GetOwner()->GetName());
        }
    }
    else if (TrapState == ETrapRoomState::Spent && bRepeatWhenEmpty && !bInside)
    {
        ResetTrap();
    }
}

void UTrapRoomComponent::StartWarning()
{
    TrapState = ETrapRoomState::Warning;
    bLossReported = false;
    SetWarningLights(true);

    GetWorld()->GetTimerManager().SetTimer(FlashTimer, this, &UTrapRoomComponent::ToggleWarningLight, FMath::Max(0.05f, FlashInterval), true);

    if (WarningDuration <= 0.0f)
    {
        StartFire();
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(WarningTimer, this, &UTrapRoomComponent::StartFire, WarningDuration, false);
}

void UTrapRoomComponent::ToggleWarningLight()
{
    if (TrapState == ETrapRoomState::Warning)
    {
        SetWarningLights(!bLightOn);
    }
}

void UTrapRoomComponent::StartFire()
{
    if (TrapState != ETrapRoomState::Warning || !CanRunTrap())
    {
        ResetTrap();
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(FlashTimer);
    TrapState = ETrapRoomState::Firing;
    SetWarningLights(true);
    SetFlames(true);

    // Burn only this room's card pickups, even if the player escaped the warning.
    DestroyRoomCards();

    GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &UTrapRoomComponent::FinishFire, FMath::Max(0.1f, FireDuration), false);
    CheckPlayer();
}

void UTrapRoomComponent::FinishFire()
{
    SetFlames(false);
    SetWarningLights(false);
    TrapState = ETrapRoomState::Spent;
}

void UTrapRoomComponent::ResetTrap()
{
    GetWorld()->GetTimerManager().ClearTimer(WarningTimer);
    GetWorld()->GetTimerManager().ClearTimer(FlashTimer);
    GetWorld()->GetTimerManager().ClearTimer(FireTimer);
    SetFlames(false);
    SetWarningLights(false);
    TrapState = ETrapRoomState::Idle;
    bLossReported = false;
}

void UTrapRoomComponent::SetWarningLights(bool bOn)
{
    bLightOn = bOn;
    const FLinearColor Red(1.0f, 0.0f, 0.0f, 1.0f);
    const float Strength = bOn ? FMath::Max(0.0f, TrapState == ETrapRoomState::Firing ? FireGlow : WarningGlow) : 0.0f;

    for (const TWeakObjectPtr<ULightComponent>& Light : WarningLights)
    {
        if (Light.IsValid())
        {
            Light->SetLightColor(Red);
            Light->SetVisibility(bOn);
        }
    }

    for (UMaterialInstanceDynamic* Material : WarningMaterials)
    {
        if (IsValid(Material))
        {
            Material->SetVectorParameterValue(LightColorParameter, Red);
            Material->SetScalarParameterValue(LightStrengthParameter, Strength);
        }
    }
}

void UTrapRoomComponent::SetFlames(bool bOn)
{
    for (const TWeakObjectPtr<UActorComponent>& Component : FlameComponents)
    {
        if (!Component.IsValid())
        {
            continue;
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component.Get()))
        {
            SceneComponent->SetVisibility(bOn, true);
        }

        if (bOn)
        {
            Component->Activate(true);
        }
        else
        {
            Component->Deactivate();
        }
    }
}

void UTrapRoomComponent::DestroyRoomCards()
{
    TArray<AActor*> Actors;
    GetOwner()->GetAllChildActors(Actors, true);

    TArray<AActor*> AttachedActors;
    GetOwner()->GetAttachedActors(AttachedActors, true, true);

    for (AActor* Actor : AttachedActors)
    {
        Actors.AddUnique(Actor);
    }

    for (AActor* Actor : Actors)
    {
        if (IsValid(Actor) && Cast<ACardPickup>(Actor))
        {
            Actor->Destroy();
        }
    }
}

void UTrapRoomComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bTrapReady = false;

    if (GetWorld())
    {
        // Maze retries and checkpoint reloads must not leave an old trap timer running.
        GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    }

    SetFlames(false);
    SetWarningLights(false);
    Super::EndPlay(EndPlayReason);
}