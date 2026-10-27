// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Decoy.generated.h"

/**
 * A fake "player" that enemies chase. It pushes itself onto the AI target stack
 * (UAITargetStack) on BeginPlay and removes itself on EndPlay, so when it expires the
 * enemies fall back to the previous target on the stack (the real player).
 */
UCLASS(Blueprintable)
class AM_2610_API ADecoy : public AActor
{
	GENERATED_BODY()

public:
	ADecoy();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoy")
	USceneComponent* Root;

	// Placeholder visual - swap the mesh in BP_Decoy. No collision, so enemies and the
	// player don't bump into it.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoy")
	UStaticMeshComponent* Mesh;

	// Seconds before the decoy removes itself. 0 = never expires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoy")
	float Lifetime = 8.f;
};
