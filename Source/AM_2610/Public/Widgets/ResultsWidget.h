// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultsWidget.generated.h"

/**
 *
 */
UCLASS()
class AM_2610_API UResultsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// Switches ResultSwitcher to its "You Win" page and hides ButtonBox.
	// The lose page enables Play Again only when the current death allows a respawn.
	UFUNCTION(BlueprintCallable, Category = "Results")
	void WinConditionMet();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UWidgetSwitcher* ResultSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* PlayAgainButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* ReturnToMainMenu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UVerticalBox* ButtonBox;

	// Cached once in NativeConstruct
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	class UBaseGameInstance* GameInstance;

	// How long the win screen stays up before automatically returning to the main menu
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Results")
	float TimeBeforeReturnToMenu = 5.0f;

private:
	TWeakObjectPtr<class UMazeSaveSubsystem> MazeSaveSystem;

	UFUNCTION()
	void HandlePlayAgainClicked();

	UFUNCTION()
	void HandleReturnToMainMenuClicked();

	UFUNCTION()
	void HandleRespawnStateChanged(bool bCanRespawn);

	UFUNCTION()
	void LoadMainMenu();

	UFUNCTION()
	void HandleReturnToMenuTimerFinished();

	FTimerHandle ReturnToMenuTimerHandle;
};
