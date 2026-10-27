#include "Widgets/KeyBindRow.h"
#include "Both/ButtonWithText.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameplayTagContainer.h"
#include "Engine/LocalPlayer.h"

void UKeyBindRow::NativeConstruct()
{
	Super::NativeConstruct();

	if (KeyButton)
	{
		KeyButton->OnBackgroundButtonClicked.AddDynamic(this, &UKeyBindRow::HandleKeyButtonClicked);
	}
}
void UKeyBindRow::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	if (bListening)
	{
		StopListening();
	}
}

void UKeyBindRow::Setup(FName InMappingName, const FText& InLabel, const FKey& InKey)
{
	MappingName = InMappingName;
	CurrentKey = InKey;

	if (ActionLabel) ActionLabel->SetText(InLabel);
	if (KeyButton)   KeyButton->SetText(CurrentKey.GetDisplayName());
}

void UKeyBindRow::HandleKeyButtonClicked()
{
	bListening = true;
	if (KeyButton)
	{
		KeyButton->SetText(NSLOCTEXT("KeyBindRow", "PressKey", "Press a key..."));
	}
}

void UKeyBindRow::StopListening()
{
	bListening = false;
	if (KeyButton)
	{
		KeyButton->SetText(CurrentKey.GetDisplayName());
	}
}

FReply UKeyBindRow::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bListening)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape)
	{
		StopListening();
		return FReply::Handled();
	}

	// Keyboard only for now - swallow everything else so it doesn't leak into the UI
	if (Key.IsGamepadKey() || Key.IsMouseButton())
	{
		return FReply::Handled();
	}

	TryRebind(Key);
	return FReply::Handled();
}

void UKeyBindRow::TryRebind(const FKey& NewKey)
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputUserSettings* Settings = Subsystem ? Subsystem->GetUserSettings() : nullptr;
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("UKeyBindRow::TryRebind - no UEnhancedInputUserSettings (enable User Settings in Project Settings > Enhanced Input)"));
		StopListening();
		return;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;

	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);
	if (FailureReason.IsEmpty())
	{
		CurrentKey = NewKey;
		Settings->ApplySettings();
		Settings->SaveSettings();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UKeyBindRow::TryRebind - MapPlayerKey failed: %s"), *FailureReason.ToString());
	}

	StopListening();
}
