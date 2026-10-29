// Fill out your copyright notice in the Description page of Project Settings.


#include "Utility/HealthComponent.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
 
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
 
	CurrentHealth = MaxHealth;
 
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleDamage);
	}
}
 
void UHealthComponent::HandleDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage == 0.0f || CurrentHealth <= 0.0f)
	{
		return;
	}
 
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);
	float Percent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
 
	if (CurrentHealth <= 0.0f)
	{
		OnDeath.Broadcast(Percent);
	}
	else if (Damage > 0.0f)
	{
		OnHurt.Broadcast(Percent);
	}
	else 
	{
		OnHeal.Broadcast(Percent);
	}
}
