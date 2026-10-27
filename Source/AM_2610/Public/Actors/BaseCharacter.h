// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "BaseCharacter.generated.h"

UCLASS(Abstract)
class AM_2610_API ABaseCharacter : public ACharacter
	, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// -------------------------------------------------------------------
	// ---- IGenericTeamAgentInterface 
	//---------------------------------------------------------------------
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	bool bIsDead = false;

	//Weapon Class Variable
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<class AActor> WeaponClass;

	// Child actor component that holds the spawned weapon, attached to the "PlaceWeaponHere" socket
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class UChildActorComponent* WeaponComponent;

	// Pointer to the spawned weapon, cast from WeaponComponent's child actor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class ARifle* WeaponReference;

	// Pointer to this character's anim instance, cast from GetMesh()->GetAnimInstance()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	class UCharacterAnimation* AnimationReference;

	// Shared by BasePlayer and Agent - handles taking damage and broadcasting health changes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	class UHealthComponent* HealthComponent;

	UFUNCTION()
	virtual void HandleHurt(float Percent);

	UFUNCTION()
	virtual void HandleDeath(float Percent);

	UFUNCTION()
	void HandleReloadNow();

	UFUNCTION()
	void HandleReloadEnded();

	// Editable per instance in the level. Same number = friendly, different = hostile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	uint8 TeamNumber = 1;

private:
	void SetupCollision();
};
