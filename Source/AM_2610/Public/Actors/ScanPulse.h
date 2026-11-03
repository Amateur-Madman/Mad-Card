#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "ScanPulse.generated.h"

UCLASS()
class AM_2610_API AScanPulse : public AActor
{
	GENERATED_BODY()

public:
	AScanPulse();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan")
	TObjectPtr<USceneComponent> Root;

	// Set the looping hum sound and its attenuation on this component in BP_ScanPulse
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan|Audio")
	TObjectPtr<UAudioComponent> HumComponent;

	// Played at each enemy the moment the wave reaches it
	UPROPERTY(EditDefaultsOnly, Category = "Scan|Audio")
	TObjectPtr<USoundBase> PingSound;

	UPROPERTY(EditDefaultsOnly, Category = "Scan|Audio", meta = (ClampMin = "0.0"))
	float HumFadeOutTime = 0.75f;

	// Unreal units (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Scan", meta = (ClampMin = "1.0"))
	float MaxRadius = 2000.0f;

	// Unreal units per second
	UPROPERTY(EditDefaultsOnly, Category = "Scan", meta = (ClampMin = "1.0"))
	float Speed = 1200.0f;

	// How long each enemy stays revealed once the wave hits it
	UPROPERTY(EditDefaultsOnly, Category = "Scan", meta = (ClampMin = "0.1"))
	float RevealDuration = 3.0f;

private:
	void RevealActor(TWeakObjectPtr<AActor> Target);
	void EndSweep();

	FTimerHandle EndSweepTimer;
};