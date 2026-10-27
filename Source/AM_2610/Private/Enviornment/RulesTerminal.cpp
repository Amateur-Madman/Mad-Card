// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/RulesTerminal.h"
#include "Enviornment/RulesSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

ARulesTerminal::ARulesTerminal()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    SetRootComponent(InteractionBox);
    InteractionBox->SetBoxExtent(FVector(80.0f, 40.0f, 100.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionObjectType(ECC_GameTraceChannel1);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionBox->SetGenerateOverlapEvents(true);
    InteractionBox->SetCanEverAffectNavigation(false);

    TerminalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalMesh"));
    TerminalMesh->SetupAttachment(InteractionBox);
    TerminalMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ARulesTerminal::BeginPlay()
{
    Super::BeginPlay();

    if (bShowRulesOnLevelLoad)
    {
        if (URulesSubsystem* Rules = URulesSubsystem::Get(this))
        {
            Rules->RequestStartupRules(RulesMenuClass);
        }
    }
}

bool ARulesTerminal::CanInteract_Implementation(AActor* Interactor)
{
    URulesSubsystem* Rules = URulesSubsystem::Get(this);
    return IsValid(Rules) && Rules->CanOpenRules(Cast<APawn>(Interactor));
}

FInteractPrompt ARulesTerminal::GetInteractPrompt_Implementation(AActor* Interactor)
{
    FInteractPrompt Prompt;
    Prompt.Slot = InteractSlot;
    Prompt.Verb = NSLOCTEXT("RulesTerminal", "ReadRules", "Read rules");
    Prompt.Target = this;
    return Prompt;
}

void ARulesTerminal::Interact_Implementation(AActor* Interactor)
{
    if (CanInteract_Implementation(Interactor))
    {
        URulesSubsystem::Get(this)->OpenRules(Cast<APawn>(Interactor), RulesMenuClass);
    }
}