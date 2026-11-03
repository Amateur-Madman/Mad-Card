// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Utility/EnemyBehaviorInterface.h"
#include "Utility/ScannableInterface.h"
#include "Agent.generated.h"

/**
 *
 */
UCLASS()
class AM_2610_API AAgent : public ACharacter, public IScannableInterface
{
	GENERATED_BODY()

public:
	AAgent();

	//----------------------------------------------------------------------//
	// IScannableInterface
	//----------------------------------------------------------------------//
	virtual void ScanRevealed_Implementation(float Duration) override;
protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UChildActorComponent* ChildActorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* ProximitySphere;

	UPROPERTY(BlueprintReadOnly, Category = Default)
	class UCharacterAnimation* CharacterAnimation;

	// Overlay material used while revealed (MI_Scan_Test). Set per enemy Blueprint.
	UPROPERTY(EditDefaultsOnly, Category = "Scan")
	TObjectPtr<UMaterialInterface> ScanOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Scan")
	FName StartTimeParameter = TEXT("RevealStartTime");

	UPROPERTY(EditDefaultsOnly, Category = "Scan")
	FName EndTimeParameter = TEXT("RevealEndTime");

private:
	void EndScan();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanMID;

	FTimerHandle ScanEndTimer;
};
