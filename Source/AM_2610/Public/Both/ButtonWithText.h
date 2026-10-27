// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ButtonWithText.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonWithTextClicked);

/**
 * Menu button with a text label and a brush-stroke selector underline.
 * The selector and bright label color show while the button is hovered or focused.
 */
UCLASS()
class AM_2610_API UButtonWithText : public UUserWidget
{
	GENERATED_BODY()

public:
	// Updates the displayed label - callable at runtime (e.g. to relabel a menu button)
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetText(const FText& NewText);

	// Shows/hides the selector underline and swaps the label color
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetSelected(bool bNewSelected);

	UFUNCTION(BlueprintPure, Category = "Button")
	bool IsSelected() const { return bSelected; }

	// Gives keyboard/gamepad focus to the inner button (use this to pre-select the first menu entry)
	UFUNCTION(BlueprintCallable, Category = "Button")
	void FocusButton();

	// Broadcast when BackgroundButton is clicked
	UPROPERTY(BlueprintAssignable, Category = "Button")
	FOnButtonWithTextClicked OnBackgroundButtonClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	// Focus path callbacks fire when focus lands on / leaves the inner button (keyboard, gamepad or hover-focus)
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UButton* BackgroundButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Information;

	// Brush-stroke underline. Optional so existing Blueprints still compile - name it Img_Selector
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UImage* Img_Selector;

	// Optional fade animation for the selector. Name it SelectorFade (animate Img_Selector opacity 0 -> 1)
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetAnimOptional))
	class UWidgetAnimation* SelectorFade;

	// Label text - set in the Designer or via SetText() at runtime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText Info;

	// Label color when not selected (dim gray)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|Style")
	FLinearColor NormalColor = FLinearColor(0.54f, 0.52f, 0.50f, 1.f);

	// Label color when selected (near white)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|Style")
	FLinearColor SelectedColor = FLinearColor(0.95f, 0.93f, 0.91f, 1.f);

	// Move keyboard focus to this button when the mouse hovers it, so only one button is ever selected
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|Style")
	bool bFocusOnHover = true;

	// Playback speed of SelectorFade (1.0 = authored length)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|Style")
	float SelectorFadeSpeed = 1.f;

private:
	UFUNCTION()
	void HandleClickedButton();

	UFUNCTION()
	void HandleHoveredButton();

	void ApplySelectionVisuals(bool bAnimate);

	bool bSelected = false;
};