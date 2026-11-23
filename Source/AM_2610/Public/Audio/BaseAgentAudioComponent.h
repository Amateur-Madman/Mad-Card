// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseAgentAudioComponent.generated.h"


UCLASS( ClassGroup=(Audio), meta=(BlueprintSpawnableComponent) )
class AM_2610_API UBaseAgentAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBaseAgentAudioComponent();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Footsteps")
	TArray<TObjectPtr<USoundWave>> FootstepWaves;

	UFUNCTION(BlueprintCallable, Category = "Audio|Movement")
	void PlayFootstep();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USoundCue> GeneratedFootstepCue;

	void BuildFootstepSoundCue();
};
