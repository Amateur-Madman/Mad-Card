// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazeSaveTable.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Enviornment/MazeSaveMenu.h"
#include "Enviornment/MazeActors.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AMazeSaveTable::AMazeSaveTable()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    SetRootComponent(InteractionBox);
    InteractionBox->SetBoxExtent(FVector(80.0f, 80.0f, 80.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionObjectType(ECC_GameTraceChannel1);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionBox->SetGenerateOverlapEvents(true);
    InteractionBox->SetCanEverAffectNavigation(false);

    TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
    TableMesh->SetupAttachment(InteractionBox);
    TableMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

AMazeActors* AMazeSaveTable::FindSaveRoom() const
{
    if (IsValid(SaveRoom))
    {
        return SaveRoom;
    }

    AActor* Parent = GetParentActor();

    while (IsValid(Parent))
    {
        if (AMazeActors* Room = Cast<AMazeActors>(Parent))
        {
            return Room;
        }

        Parent = Parent->GetParentActor();
    }

    return nullptr;
}

bool AMazeSaveTable::CanInteract_Implementation(AActor* Interactor)
{
    UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
    APawn* Player = Cast<APawn>(Interactor);
    AMazeActors* Room = FindSaveRoom();
    return IsValid(SaveSystem) && SaveSystem->IsCurrentStartRoom(Room) && SaveSystem->CanSaveAtTable(Player);
}

FInteractPrompt AMazeSaveTable::GetInteractPrompt_Implementation(AActor* Interactor)
{
    FInteractPrompt Prompt;
    Prompt.Slot = InteractSlot;
    Prompt.Verb = NSLOCTEXT("MazeSaveTable", "SaveGame", "Save game");
    Prompt.Target = this;
    return Prompt;
}

void AMazeSaveTable::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    UMazeSaveMenu::OpenSaveMenu(this, EMazeSaveMenuMode::Save, Cast<APawn>(Interactor), SaveMenuClass);
}
