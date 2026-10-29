// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Decoy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Utility/AITargetStack.h"

ADecoy::ADecoy()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADecoy::BeginPlay()
{
	Super::BeginPlay();

	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}

	if (UWorld* World = GetWorld())
	{
		if (UAITargetStack* Stack = World->GetSubsystem<UAITargetStack>())
		{
			Stack->Push(this);
		}
	}
}

void ADecoy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAITargetStack* Stack = World->GetSubsystem<UAITargetStack>())
		{
			Stack->Remove(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
