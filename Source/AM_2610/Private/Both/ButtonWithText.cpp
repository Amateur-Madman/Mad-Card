// Fill out your copyright notice in the Description page of Project Settings.

#include "Both/ButtonWithText.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"

void UButtonWithText::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Information)
	{
		Information->SetText(Info);
	}

	// Show the unselected look in the Designer preview and on first construction
	ApplySelectionVisuals(false);
}

void UButtonWithText::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackgroundButton)
	{
		BackgroundButton->OnClicked.AddDynamic(this, &UButtonWithText::HandleClickedButton);
		BackgroundButton->OnHovered.AddDynamic(this, &UButtonWithText::HandleHoveredButton);
	}
}

void UButtonWithText::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	SetSelected(true);
}

void UButtonWithText::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	SetSelected(false);
}

void UButtonWithText::SetText(const FText& NewText)
{
	Info = NewText;
	if (Information)
	{
		Information->SetText(Info);
	}
}

void UButtonWithText::SetSelected(bool bNewSelected)
{
	if (bSelected == bNewSelected)
	{
		return;
	}

	bSelected = bNewSelected;
	ApplySelectionVisuals(true);
}

void UButtonWithText::FocusButton()
{
	if (BackgroundButton)
	{
		BackgroundButton->SetKeyboardFocus();
	}
}

void UButtonWithText::ApplySelectionVisuals(bool bAnimate)
{
	if (Information)
	{
		Information->SetColorAndOpacity(FSlateColor(bSelected ? SelectedColor : NormalColor));
	}

	if (bAnimate && SelectorFade)
	{
		// Forward when selecting, reverse when deselecting, continuing from the current point
		PlayAnimation(SelectorFade, 0.f, 1,
			bSelected ? EUMGSequencePlayMode::Forward : EUMGSequencePlayMode::Reverse,
			SelectorFadeSpeed);
	}
	else if (Img_Selector)
	{
		// No animation (or initial setup): snap the opacity
		Img_Selector->SetRenderOpacity(bSelected ? 1.f : 0.f);
	}
}

void UButtonWithText::HandleClickedButton()
{
	OnBackgroundButtonClicked.Broadcast();
}

void UButtonWithText::HandleHoveredButton()
{
	if (bFocusOnHover)
	{
		// Focus drives selection, so hovering one button deselects whichever other one had focus
		FocusButton();
	}
	else
	{
		SetSelected(true);
	}
}