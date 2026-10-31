#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

// Audio and brightness settings persisted to disk. Key bindings are NOT stored here - Enhanced Input's
// UEnhancedInputUserSettings saves those itself. Other graphics settings live in GameUserSettings.ini.
UCLASS()
class AM_2610_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float MusicVolume = 1.f;
	UPROPERTY() float SFXVolume = 1.f;
	UPROPERTY() float DialogueVolume = 1.f;
	UPROPERTY() float Brightness = 0.5f; // normalized 0..1, 0.5 = engine default gamma (2.2)

	static constexpr float MinGamma = 1.6f;
	static constexpr float MaxGamma = 2.8f;

	static float NormalizedToGamma(float N) { return FMath::Lerp(MinGamma, MaxGamma, FMath::Clamp(N, 0.f, 1.f)); }
	static float GammaToNormalized(float G) { return FMath::Clamp((G - MinGamma) / (MaxGamma - MinGamma), 0.f, 1.f); }

	static void ApplyBrightness(float Normalized)
	{
		if (GEngine) { GEngine->DisplayGamma = NormalizedToGamma(Normalized); }
	}
};