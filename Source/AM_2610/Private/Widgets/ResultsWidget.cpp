// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ResultsWidget.h"
#include "Both/ButtonWithText.h"
#include "Utility/BaseGameInstance.h"
#include "Components/WidgetSwitcher.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Enviornment/MazeSaveSubsystem.h"

void UResultsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GameInstance = Cast<UBaseGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("UResultsWidget::NativeConstruct - GameInstance is not a UBaseGameInstance"));
	}

	if (PlayAgainButton)
	{
		PlayAgainButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UResultsWidget::HandlePlayAgainClicked);
	}

	if (ReturnToMainMenu)
	{
		ReturnToMainMenu->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UResultsWidget::HandleReturnToMainMenuClicked);
	}

	MazeSaveSystem = UMazeSaveSubsystem::Get(this);
	if (UMazeSaveSubsystem* SaveSystem = MazeSaveSystem.Get())
	{
		// OnPlayerLost listeners can create this widget before or after the death is recorded.
		SaveSystem->OnRespawnStateChanged.AddUniqueDynamic(this, &UResultsWidget::HandleRespawnStateChanged);
		HandleRespawnStateChanged(SaveSystem->CanRespawnPlayer());
	}
	else
	{
		HandleRespawnStateChanged(false);
	}
}

void UResultsWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToMenuTimerHandle);
	}

	if (PlayAgainButton)
	{
		PlayAgainButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UResultsWidget::HandlePlayAgainClicked);
	}

	if (ReturnToMainMenu)
	{
		ReturnToMainMenu->OnBackgroundButtonClicked.RemoveDynamic(this, &UResultsWidget::HandleReturnToMainMenuClicked);
	}

	if (UMazeSaveSubsystem* SaveSystem = MazeSaveSystem.Get())
	{
		SaveSystem->OnRespawnStateChanged.RemoveDynamic(this, &UResultsWidget::HandleRespawnStateChanged);
	}

	MazeSaveSystem.Reset();
	Super::NativeDestruct();
}

void UResultsWidget::WinConditionMet()
{
	if (ResultSwitcher)
	{
		ResultSwitcher->SetActiveWidgetIndex(1);
	}

	if (ButtonBox)
	{
		ButtonBox->SetVisibility(ESlateVisibility::Hidden);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReturnToMenuTimerHandle, this, &UResultsWidget::HandleReturnToMenuTimerFinished, TimeBeforeReturnToMenu, false);
	}
}

void UResultsWidget::HandlePlayAgainClicked()
{
	if (UMazeSaveSubsystem* SaveSystem = MazeSaveSystem.Get())
	{
		// Keep the current maze count, completed puzzles, and unlocked start-room doors.
		SaveSystem->RespawnPlayer();
	}
}

void UResultsWidget::HandleReturnToMainMenuClicked()
{
	if (UMazeSaveSubsystem* SaveSystem = MazeSaveSystem.Get())
	{
		SaveSystem->ReturnToMainMenu();
		return;
	}

	if (GameInstance)
	{
		GameInstance->QuitToMenu();
	}
}

void UResultsWidget::HandleRespawnStateChanged(bool bCanRespawn)
{
	const UMazeSaveSubsystem* SaveSystem = MazeSaveSystem.Get();
	if (PlayAgainButton)
	{
		PlayAgainButton->SetIsEnabled(bCanRespawn);
	}

	if (ReturnToMainMenu)
	{
		ReturnToMainMenu->SetIsEnabled(IsValid(SaveSystem) ? SaveSystem->CanReturnToMainMenu() : IsValid(GameInstance));
	}

	if (SecondChancesText)
	{
		const bool bShowCount = IsValid(SaveSystem) && SaveSystem->GetDifficulty() == EMazeDifficulty::Hard;
		if (bShowCount)
		{
			// Count the retry granted for this death along with any unused charges.
			const int32 SecondChancesLeft = SaveSystem->GetSecondChanceCharges() + (bCanRespawn ? 1 : 0);
			SecondChancesText->SetText(FText::Format(
				NSLOCTEXT("ResultsWidget", "SecondChancesLeft", "Second Chances Left: {0}"),
				FText::AsNumber(SecondChancesLeft)));
		}
		SecondChancesText->SetVisibility(bShowCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UResultsWidget::LoadMainMenu()
{
	HandleReturnToMainMenuClicked();
}

void UResultsWidget::HandleReturnToMenuTimerFinished()
{
	HandleReturnToMainMenuClicked();
}