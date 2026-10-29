#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AudioSettingsPage.generated.h"

UCLASS()
class AM_2610_API UAudioSettingsPage : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// Sliders must be configured 0..1 in the Designer
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class USlider* MasterSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class USlider* MusicSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class USlider* SFXSlider;	
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class USlider* DialogueSlider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	class UBaseGameInstance* CurrentGameInstance;

private:
	UFUNCTION() void HandleMasterChanged(float Value);
	UFUNCTION() void HandleMusicChanged(float Value);
	UFUNCTION() void HandleSFXChanged(float Value);
	UFUNCTION() void HandleDialogueChanged(float Value);

	// Fired when the user lets go of any slider - writes the save file
	UFUNCTION() void HandleSliderReleased();
};
