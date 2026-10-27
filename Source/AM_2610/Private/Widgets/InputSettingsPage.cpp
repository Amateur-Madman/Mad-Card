#include "Widgets/InputSettingsPage.h"
#include "Widgets/KeyBindRow.h"
#include "Both/ButtonWithText.h"
#include "Components/PanelWidget.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

namespace
{
	UEnhancedInputUserSettings* GetUserSettings(const UUserWidget* Widget)
	{
		ULocalPlayer* LP = Widget->GetOwningLocalPlayer();
		UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		return Subsystem ? Subsystem->GetUserSettings() : nullptr;
	}
}

void UInputSettingsPage::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Warning, TEXT("[InputPage] NativeConstruct - IMC=%s RowClass=%s RowContainer=%s Reset=%s"),
		*GetNameSafe(MappingContext), *GetNameSafe(KeyBindRowClass),
		RowContainer ? TEXT("valid") : TEXT("NULL"), ResetButton ? TEXT("valid") : TEXT("NULL"));

	if (ResetButton)
	{
		ResetButton->OnBackgroundButtonClicked.AddDynamic(this, &UInputSettingsPage::HandleResetClicked);
	}

	RebuildRows();
}

void UInputSettingsPage::RebuildRows()
{
	if (!RowContainer || !KeyBindRowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputPage] RebuildRows ABORT - RowContainer or KeyBindRowClass missing"));
		return;
	}
	RowContainer->ClearChildren();

	ULocalPlayer* LP = GetOwningLocalPlayer();
	UE_LOG(LogTemp, Warning, TEXT("[InputPage] OwningPlayer=%s LocalPlayer=%s"),
		*GetNameSafe(GetOwningPlayer()), *GetNameSafe(LP));

	UEnhancedInputUserSettings* Settings = GetUserSettings(this);
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputPage] ABORT - no UEnhancedInputUserSettings (no LocalPlayer, or Enable User Settings is off)"));
		return;
	}

	if (MappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputPage] IMC %s has %d total mappings"),
			*MappingContext->GetName(), MappingContext->GetMappings().Num());
		Settings->RegisterInputMappingContext(MappingContext);
	}

	UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile();
	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputPage] ABORT - no active key profile"));
		return;
	}

	const int32 RowCount = Profile->GetPlayerMappingRows().Num();
	UE_LOG(LogTemp, Warning, TEXT("[InputPage] Profile has %d mapping rows"), RowCount);

	int32 Created = 0;
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			if (Mapping.GetSlot() != EPlayerMappableKeySlot::First)
			{
				continue;
			}

			if (UKeyBindRow* Row = CreateWidget<UKeyBindRow>(GetOwningPlayer(), KeyBindRowClass))
			{
				Row->Setup(Mapping.GetMappingName(), Mapping.GetDisplayName(), Mapping.GetCurrentKey());
				RowContainer->AddChild(Row);
				++Created;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[InputPage] CreateWidget failed for %s"), *Mapping.GetMappingName().ToString());
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[InputPage] Created %d row widgets"), Created);
}
void UInputSettingsPage::HandleResetClicked()
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings(this))
	{
		if (UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile())
		{
			Profile->ResetToDefault();
			Settings->ApplySettings();
			Settings->SaveSettings();
		}
	}
	RebuildRows();
}
