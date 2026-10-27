// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, Percent);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AM_2610_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// Broadcasts remaining health as a 0-1 percent whenever damage is taken but the owner survives
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHurt;

	// Broadcasts remaining health as a 0-1 percent whenever damage is taken but the owner survives
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHeal;

	// Broadcasts (with percent, always 0) when health reaches zero
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnDeath;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, Category = "Health")
	float CurrentHealth = 0.0f;

	// Bound to the owner's OnTakeAnyDamage - never called directly from outside this component
	UFUNCTION()
	void HandleDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);
};
