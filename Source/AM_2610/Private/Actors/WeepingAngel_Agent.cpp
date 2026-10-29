// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WeepingAngel_Agent.h"
#include "Components/SphereComponent.h"
#include "Actors/BaseGameMode.h"
#include "Actors/BasePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Both/WeepAnimation.h"
#include "../AM_2610.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Actors/WeepingAngelController.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

AWeepingAngel_Agent::AWeepingAngel_Agent()
{
	CanKillPlayer = true;

	StimComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimulus"));
}

//Seen by player
void AWeepingAngel_Agent::PlayerSeesMe_Implementation(bool bIsSeen)
{
	if (AWeepingAngelController* AIC = Cast<AWeepingAngelController>(GetController()))
	{
		if (UBlackboardComponent* BBComp = AIC->GetBlackboardComponent())
		{
			BBComp->SetValueAsBool(TEXT("SeesAgent"), bIsSeen);
		}
		GetMesh()->PrimaryComponentTick.SetTickFunctionEnable(!bIsSeen);
		CanKillPlayer = !bIsSeen;
	}
}

void AWeepingAngel_Agent::BeginPlay()
{
	Super::BeginPlay();
	if (ProximitySphere)
	{
		ProximitySphere->OnComponentBeginOverlap.AddDynamic(this, &AWeepingAngel_Agent::OnProximityOverlapBegin);
	}

	if (StimComponent)
	{
		StimComponent->RegisterForSense(UAISense_Sight::StaticClass());
		StimComponent->RegisterWithPerceptionSystem();
	}

	GetCharacterMovement()->MaxWalkSpeed = 450.0f;
}

void AWeepingAngel_Agent::OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if (ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
			if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(GetWorld()->GetAuthGameMode()))
			{
				if(CanKillPlayer)
				GameMode->TriggerLose();
			}
		}
	}
}