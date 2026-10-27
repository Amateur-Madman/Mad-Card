// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rifle.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRifle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, float, Current, float, Max);

UCLASS()
class AM_2610_API ARifle : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARifle();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Fires the rifle - spawns a projectile at the weapon socket
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Attack();

	// Starts a reload if one isn't already in progress (or a shot isn't on cooldown) -
	// locks bActionHappening and broadcasts OnReloadStart so the character can kick off
	// the reload animation. No-ops if already busy.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void RequestReload();
	// Refills CurrentAmmo to MaxAmmo and broadcasts OnAmmoChanged
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ReloadAmmo();

	void SetAlive(bool bAlive);

	// Bound to the cooldown timer - clears bActionHappening
	UFUNCTION()
	void ActionStopped();

	// Increases MaxAmmo by Amount and broadcasts the new max via OnAmmoChanged - CurrentAmmo
	// is left untouched, so the HUD's max number updates without granting free ammo (requirement 4)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void IncreaseMaxAmmo(float Amount);
protected:
	// Pointer to the character that owns this rifle, cast from GetParentActor()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class ABaseCharacter* OwningCharacter;

	// Name of the socket on the owning character's mesh to spawn projectiles from
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	FName WeaponSocketName = "MuzzleSocket";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<class AProjectile> ProjectileClass;

	// Ammo capacity - CurrentAmmo is reset to this by ReloadAmmo()
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float MaxAmmo = 3.0f;



	// How long (seconds) the rifle stays on cooldown after firing
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ResetTime = 2.0f;

	FTimerHandle ResetTimeHandle;



	void UseAmmo();

private:
	// Give Access to the bIsAlive variable to the owning character, so it can be set to false when the character dies
	UPROPERTY()
	bool bIsAlive = true;

	// Starts at 0 - only populated once something calls ReloadAmmo()
	UPROPERTY( )
	float CurrentAmmo = 0.0f;

	// Cooldown gate - true while a shot is on cooldown
	UPROPERTY()
	bool bActionHappening = false;

	// True when the rifle is not on cooldown
	UFUNCTION()
	bool CanShoot() const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnRifle OnRifleAttack;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnRifle OnActionStopped;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnRifle OnReloadStart;
};
