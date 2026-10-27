// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Agent.h"
#include "Components/SphereComponent.h"
#include "Both/CharacterAnimation.h"
#include "../AM_2610.h"

AAgent::AAgent()
{
	PrimaryActorTick.bCanEverTick = false;

	ChildActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ChildActorComponent"));
	if(ChildActorComponent)
	{
		ChildActorComponent->SetupAttachment(GetRootComponent());
	}

	ProximitySphere = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySphere"));
	if(ProximitySphere)
	{
		ProximitySphere->SetupAttachment(GetRootComponent());
	}

	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.f, 0.0f));
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
}

void AAgent::BeginPlay()
{
	Super::BeginPlay();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance)
	{
		CharacterAnimation = Cast<UCharacterAnimation>(AnimInstance);
	}
}