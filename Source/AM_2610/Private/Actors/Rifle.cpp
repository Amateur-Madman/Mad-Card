// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Rifle.h"
#include "Actors/BaseCharacter.h"
#include "Actors/BasePlayer.h"
#include "Actors/Projectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "../AM_2610.h"

// Sets default values
ARifle::ARifle()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ARifle::BeginPlay()
{
	Super::BeginPlay();

	// Check the cast result BEFORE assigning to OwningCharacter (error checked before setting the parent pawn)
	ABaseCharacter* CastResult = Cast<ABaseCharacter>(GetParentActor());
	if (!CastResult)
	{
		UE_LOG(LogTemp, Warning, TEXT("Need a pawn to own this rifle"));
		Destroy();
		return;
	}

	OwningCharacter = CastResult;

	ReloadAmmo();
}

bool ARifle::CanShoot() const
{
	return !bActionHappening && bIsAlive && CurrentAmmo > 0.0f;
}

void ARifle::ActionStopped()
{
	bActionHappening = false;
	OnActionStopped.Broadcast();
}
void ARifle::Attack()
{
	if (!CanShoot())
	{
		return;
	}

	if (!OwningCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack: no OwningCharacter (parent cast failed at BeginPlay)"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack: ProjectileClass is not set - check BP_CodeRifle Class Defaults"));
		return;
	}

	// The rifle has its own SkeletalMesh component (added in the Blueprint), so we
	// look it up rather than declaring a duplicate one here.
	USkeletalMeshComponent* RifleMesh = FindComponentByClass<USkeletalMeshComponent>();
	if (!RifleMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack: no SkeletalMeshComponent found on this rifle"));
		return;
	}

	if (!RifleMesh->DoesSocketExist(WeaponSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack: socket '%s' does not exist on the rifle mesh"), *WeaponSocketName.ToString());
	}

	FVector SocketLocation = RifleMesh->GetSocketLocation(WeaponSocketName);

	// Aim at the exact point the crosshair is tracing, not a separate guess at it -
	// this guarantees the shot and the reticle always agree. Falls back to
	// GetBaseAimRotation() for non-player owners (e.g. AAgent) with no crosshair.
	FRotator AimRotation;
	ABasePlayer* OwningPlayer = Cast<ABasePlayer>(OwningCharacter);
	if (OwningPlayer)
	{
		AimRotation = (OwningPlayer->GetCrosshairTarget() - SocketLocation).Rotation();
	}
	else
	{
		AimRotation = OwningCharacter->GetBaseAimRotation();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetInstigatorController();
	SpawnParams.Instigator = OwningCharacter;

	AProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SocketLocation, AimRotation, SpawnParams);
	UE_LOG(LogTemp, Log, TEXT("Attack: spawned projectile = %s at %s"), SpawnedProjectile ? TEXT("SUCCESS") : TEXT("FAILED"), *SocketLocation.ToString());

	UseAmmo();

	bActionHappening = true;
	GetWorldTimerManager().SetTimer(ResetTimeHandle, this, &ARifle::ActionStopped, ResetTime, false);
	OnRifleAttack.Broadcast();
}
void ARifle::RequestReload()
{
	if (bActionHappening)
	{
		return;
	}

	bActionHappening = true;
	OnReloadStart.Broadcast();
}
void ARifle::ReloadAmmo()
{
	CurrentAmmo = MaxAmmo;
	OnAmmoChanged.Broadcast(CurrentAmmo, MaxAmmo);
}

void ARifle::UseAmmo()
{
	CurrentAmmo = FMath::Max(CurrentAmmo - 1.0f, 0.0f);
	OnAmmoChanged.Broadcast(CurrentAmmo, MaxAmmo);
}

void ARifle::SetAlive(bool bNewAlive)
{
	bIsAlive = bNewAlive;
}
// Called every frame
void ARifle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARifle::IncreaseMaxAmmo(float Amount)
{
	MaxAmmo += Amount;
	OnAmmoChanged.Broadcast(CurrentAmmo, MaxAmmo);
}