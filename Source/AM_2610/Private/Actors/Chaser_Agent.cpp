// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chaser_Agent.h"
#include "Components/SphereComponent.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"


void AChaser_Agent::BeginPlay()
{
	HardSpeed = 60.0f;
	EasySpeed = 20.0f;

	Super::BeginPlay();

	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AChaser_Agent::OnProximityOverlapBegin);
	}
}

void AChaser_Agent::OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if (ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
			if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GameMode->TriggerLose();
			}
		}
	}
}