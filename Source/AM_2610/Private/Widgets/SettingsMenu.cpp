#include "Widgets/SettingsMenu.h"
#include "Both/ButtonWithText.h"
#include "Components/WidgetSwitcher.h"
#include "Utility/BaseGameInstance.h"

void USettingsMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (InputTabButton) InputTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleInputTabClicked);
	if (AudioTabButton) AudioTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleAudioTabClicked);
	if (BackButton)     BackButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleBackClicked);

	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(0);
}

void USettingsMenu::HandleInputTabClicked() { if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(0); }
void USettingsMenu::HandleAudioTabClicked() { if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(1); }

void USettingsMenu::HandleBackClicked()
{
	if (UBaseGameInstance* GI = Cast<UBaseGameInstance>(GetGameInstance()))
	{
		GI->SaveSettings();
	}

	OnClosed.Broadcast();
	RemoveFromParent();
}
