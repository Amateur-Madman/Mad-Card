#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputSettingsPage.generated.h"

class UInputMappingContext;
class UKeyBindRow;

// Builds one UKeyBindRow per player-mappable key in MappingContext.
UCLASS()
class AM_2610_API UInputSettingsPage : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// The same IMC the player adds in BasePlayer (IMC_Default). Registered with the user
	// settings here so the page works on the main menu level where no player exists.
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* MappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TSubclassOf<UKeyBindRow> KeyBindRowClass;

	// Rows get added here (a ScrollBox is the usual choice)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UPanelWidget* RowContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UButtonWithText* ResetButton;

private:
	void RebuildRows();

	UFUNCTION()
	void HandleResetClicked();
};
