// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Utility/EnemyBehaviorInterface.h"
#include "Utility/ScannableInterface.h"
#include "Agent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFootstep);
/**
 *
 */
UCLASS()
class AM_2610_API AAgent : public ACharacter, public IScannableInterface
{
	GENERATED_BODY()

public:
	AAgent();

	UPROPERTY(BlueprintAssignable, Category = "Events|Audio")
	FOnFootstep OnFootstep;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	class UBaseAgentAudioComponent* BaseAudioComponent;

	//Difficulty Speeds
	float EasySpeed;
	float HardSpeed;

	void SetDifficultySpeeds(float InEasySpeed, float InHardSpeed) { EasySpeed = InEasySpeed; HardSpeed = InHardSpeed; }
	void SpeedAccordingToDiff();

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
