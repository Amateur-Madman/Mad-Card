// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePuzzleButton.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

AMazePuzzleButton::AMazePuzzleButton()
{
    PrimaryActorTick.bCanEverTick = false;
    ButtonArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ButtonArea"));
    SetRootComponent(ButtonArea);
    ButtonArea->SetBoxExtent(FVector(70.0f, 70.0f, 50.0f));
    ButtonArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ButtonArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    ButtonArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ButtonArea->SetGenerateOverlapEvents(true);
    ButtonArea->SetCanEverAffectNavigation(false);
}

void AMazePuzzleButton::GetOverlappingPawns(TArray<APawn*>& Pawns) const
{
    Pawns.Reset();

    if (!IsValid(ButtonArea))
    {
        return;
    }

    TArray<AActor*> Actors;
    ButtonArea->GetOverlappingActors(Actors, APawn::StaticClass());

    for (AActor* Actor : Actors)
    {
        APawn* Pawn = Cast<APawn>(Actor);
        UPrimitiveComponent* Body = IsValid(Pawn) ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;

        // Count the pawn's collision body, not extra mesh or camera overlaps.
        if (IsValid(Body) && ButtonArea->IsOverlappingComponent(Body))
        {
            Pawns.AddUnique(Pawn);
        }
    }
}

void AMazePuzzleButton::SetPressed(bool bPressed)
{
    if (bIsPressed != bPressed)
    {
        bIsPressed = bPressed;
        ButtonPressedChanged(bPressed);
    }
}
