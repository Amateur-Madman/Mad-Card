// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Utility/EnemyBehaviorInterface.h"
#include "Agent.generated.h"

/**
 *
 */
UCLASS()
class AM_2610_API AAgent : public ACharacter
{
	GENERATED_BODY()

public:
	AAgent();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UChildActorComponent* ChildActorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* ProximitySphere;

	UPROPERTY(BlueprintReadOnly, Category = Default)
	class UCharacterAnimation* CharacterAnimation;
};
