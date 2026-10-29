#include "Widgets/AudioSettingsPage.h"
#include "Components/Slider.h"
#include "Utility/BaseGameInstance.h"

void UAudioSettingsPage::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentGameInstance = Cast<UBaseGameInstance>(GetGameInstance());
	if (!CurrentGameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAudioSettingsPage::NativeConstruct - GameInstance is not a UBaseGameInstance"));
		return;
	}

	// SetValue doesn't broadcast OnValueChanged, so initialising first is safe
	MasterSlider->SetValue(CurrentGameInstance->GetMasterVolume());
	MusicSlider->SetValue(CurrentGameInstance->GetMusicVolume());
	SFXSlider->SetValue(CurrentGameInstance->GetSFXVolume());
	DialogueSlider->SetValue(CurrentGameInstance->GetSFXVolume());

	MasterSlider->OnValueChanged.AddDynamic(this, &UAudioSettingsPage::HandleMasterChanged);
	MusicSlider->OnValueChanged.AddDynamic(this, &UAudioSettingsPage::HandleMusicChanged);
	SFXSlider->OnValueChanged.AddDynamic(this, &UAudioSettingsPage::HandleSFXChanged);
	DialogueSlider->OnValueChanged.AddDynamic(this, &UAudioSettingsPage::HandleDialogueChanged);

	MasterSlider->OnMouseCaptureEnd.AddDynamic(this, &UAudioSettingsPage::HandleSliderReleased);
	MusicSlider->OnMouseCaptureEnd.AddDynamic(this, &UAudioSettingsPage::HandleSliderReleased);
	SFXSlider->OnMouseCaptureEnd.AddDynamic(this, &UAudioSettingsPage::HandleSliderReleased);
	DialogueSlider->OnMouseCaptureEnd.AddDynamic(this, &UAudioSettingsPage::HandleSliderReleased);
}

void UAudioSettingsPage::HandleMasterChanged(float Value) { if (CurrentGameInstance) CurrentGameInstance->SetMasterVolume(Value); }
void UAudioSettingsPage::HandleMusicChanged(float Value)  { if (CurrentGameInstance) CurrentGameInstance->SetMusicVolume(Value); }
void UAudioSettingsPage::HandleSFXChanged(float Value)    { if (CurrentGameInstance) CurrentGameInstance->SetSFXVolume(Value); }
void UAudioSettingsPage::HandleDialogueChanged(float Value)    { if (CurrentGameInstance) CurrentGameInstance->SetDialogueVolume(Value); }

void UAudioSettingsPage::HandleSliderReleased()
{
	if (CurrentGameInstance) CurrentGameInstance->SaveSettings();
}
