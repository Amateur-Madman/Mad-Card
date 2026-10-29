#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "KeyBindRow.generated.h"

// One line in the Input page: "<Action name>   [ Key ]". Click the key button, press a
// keyboard key to rebind, Escape cancels.
UCLASS()
class AM_2610_API UKeyBindRow : public UUserWidget
{
	GENERATED_BODY()

public:
	// Call right after CreateWidget. MappingName is the "Mapping Name" set under
	// Player Mappable Options on the IMC entry.
	void Setup(FName InMappingName, const FText& InLabel, const FKey& InKey);

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* ActionLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButtonWithText* KeyButton;

	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

private:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleKeyButtonClicked();

	void StopListening();
	void TryRebind(const FKey& NewKey);

	FName MappingName;
	FKey CurrentKey;
	bool bListening = false;
};
