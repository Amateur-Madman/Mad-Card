#include "Widgets/SettingsMenu.h"
#include "Both/ButtonWithText.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/GraphicsSettingsPage.h"
#include "Utility/BaseGameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
void USettingsMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (InputTabButton) InputTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleInputTabClicked);
	if (AudioTabButton) AudioTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleAudioTabClicked);
	if (GraphicsTabButton) GraphicsTabButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleGraphicsTabClicked);
	if (BackButton)     BackButton->OnBackgroundButtonClicked.AddDynamic(this, &USettingsMenu::HandleBackClicked);

	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(0);

	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, UUserWidget::StaticClass(), true);
	for (UUserWidget* W : Found)
	{
		if (W && W != this)
		{
			HiddenWidgets.Add(W, W->GetVisibility());
			W->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
void USettingsMenu::NativeDestruct()
{
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : HiddenWidgets)
	{
		if (Pair.Key.IsValid()) Pair.Key->SetVisibility(Pair.Value);
	}
	HiddenWidgets.Empty();
	Super::NativeDestruct();
}

// Tabs are locked while the graphics page is waiting on "keep these settings?"
void USettingsMenu::HandleInputTabClicked() { if (PageSwitcher && !(GraphicsPage && GraphicsPage->IsAwaitingConfirm())) PageSwitcher->SetActiveWidgetIndex(0); }
void USettingsMenu::HandleAudioTabClicked() { if (PageSwitcher && !(GraphicsPage && GraphicsPage->IsAwaitingConfirm())) PageSwitcher->SetActiveWidgetIndex(1); }
void USettingsMenu::HandleGraphicsTabClicked() { if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(2);if (GraphicsPage) GraphicsPage->RequestPreviewRecapture();}

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