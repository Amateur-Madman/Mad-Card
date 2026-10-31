#include "Widgets/SettingsMenu.h"
#include "Both/ButtonWithText.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/GraphicsSettingsPage.h"
#include "Utility/BaseGameInstance.h"

void USettingsMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (InputTabButton) InputTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleInputTabClicked);
	if (AudioTabButton) AudioTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleAudioTabClicked);
	if (GraphicsTabButton) GraphicsTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleGraphicsTabClicked);
	if (BackButton)     BackButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleBackClicked);

	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(0);
}

// Tabs are locked while the graphics page is waiting on "keep these settings?"
void USettingsMenu::HandleInputTabClicked() { if (PageSwitcher && !(GraphicsPage && GraphicsPage->IsAwaitingConfirm())) PageSwitcher->SetActiveWidgetIndex(0); }
void USettingsMenu::HandleAudioTabClicked() { if (PageSwitcher && !(GraphicsPage && GraphicsPage->IsAwaitingConfirm())) PageSwitcher->SetActiveWidgetIndex(1); }
void USettingsMenu::HandleGraphicsTabClicked() { if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(2); }

void USettingsMenu::HandleBackClicked()
{
	// Leaving mid-countdown counts as "not confirmed": roll the display settings back first.
	if (GraphicsPage)
	{
		GraphicsPage->CancelPendingChanges();
	}

	if (UBaseGameInstance* GI = Cast<UBaseGameInstance>(GetGameInstance()))
	{
		GI->SaveSettings();
	}

	OnClosed.Broadcast();
	RemoveFromParent();
}