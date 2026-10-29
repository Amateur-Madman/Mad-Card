#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

// Audio settings persisted to disk. Key bindings are NOT stored here - Enhanced Input's
// UEnhancedInputUserSettings saves those itself.
UCLASS()
class AM_2610_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float MusicVolume = 1.f;
	UPROPERTY() float SFXVolume = 1.f;
	UPROPERTY() float DialogueVolume = 1.f;


};
