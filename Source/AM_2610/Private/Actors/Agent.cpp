// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Agent.h"
#include "Components/SphereComponent.h"
#include "Both/CharacterAnimation.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	if(GetMesh())
	{
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.f, 0.0f));
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	}
}

void AAgent::BeginPlay()
{
	Super::BeginPlay();

	if(HardSpeed && EasySpeed)
	{
		SetDifficultySpeeds(EasySpeed, HardSpeed);
		SpeedAccordingToDiff();
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance)
	{
		CharacterAnimation = Cast<UCharacterAnimation>(AnimInstance);
	}
	if (ScanOverlayMaterial)
	{
		ScanMID = UMaterialInstanceDynamic::Create(ScanOverlayMaterial, this);
	}
}

void AAgent::ScanRevealed_Implementation(float Duration)
{
	if (!ScanMID || Duration <= 0.0f)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	ScanMID->SetScalarParameterValue(StartTimeParameter, Now);             // the material restarts its flash
	ScanMID->SetScalarParameterValue(EndTimeParameter, Now + Duration);
	GetMesh()->SetOverlayMaterial(ScanMID);

	// Calling SetTimer on an active handle resets it, so a second scan extends the reveal
	GetWorldTimerManager().SetTimer(ScanEndTimer, this, &AAgent::EndScan, Duration, false);
}

void AAgent::SpeedAccordingToDiff()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		UMazeSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UMazeSaveSubsystem>();

		if (SaveSubsystem)
		{
			if (SaveSubsystem->GetDifficulty() == EMazeDifficulty::Hard)
			{
				GetCharacterMovement()->MaxWalkSpeed = HardSpeed;
			}
			else
			{
				GetCharacterMovement()->MaxWalkSpeed = EasySpeed;
			}
		}
	}
}

void AAgent::EndScan()
{
	GetMesh()->SetOverlayMaterial(nullptr);
}