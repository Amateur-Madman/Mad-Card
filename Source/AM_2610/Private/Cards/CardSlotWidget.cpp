// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/CardSlotWidget.h"

void UCardSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Also runs in the designer, so the key label and empty state preview correctly
	if (KeyText)
	{
		KeyText->SetText(KeyLabel);
		KeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// The ghost rests invisible; UsedAnim fades it in and out
	if (CardGhost)
	{
		CardGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
		CardGhost->SetRenderOpacity(0.f);
	}

	ShowEmpty();
}

void UCardSlotWidget::ShowEmpty()
{
	if (EmptyFrame) EmptyFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (CardFrame) CardFrame->SetVisibility(ESlateVisibility::Collapsed);
	if (CardIcon) CardIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (NameText) NameText->SetVisibility(ESlateVisibility::Collapsed);

	bHasCard = false;
}

void UCardSlotWidget::SetCard(UBaseCard* Card)
{
	if (!Card)
	{
		ShowEmpty();
		return;
	}

	if (UTexture2D* Icon = Card->GetIcon())
	{
		CardIcon->SetBrushFromTexture(Icon);
		CardIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CardIcon->SetVisibility(ESlateVisibility::Collapsed); // no icon: name only
	}

	NameText->SetText(Card->GetDisplayName());
	NameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	CardFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	EmptyFrame->SetVisibility(ESlateVisibility::Collapsed);

	// Only pop in when a card arrives in an empty slot (not on every refresh)
	if (!bHasCard)
	{
		RestartAnimation(GainAnim);
	}
	bHasCard = true;
}

void UCardSlotWidget::PlayUsed(UBaseCard* Card)
{
	if (!Card || !Card->GetIcon())
	{
		return;
	}

	CardGhost->SetBrushFromTexture(Card->GetIcon());
	RestartAnimation(UsedAnim);
}

void UCardSlotWidget::PlayRefused()
{
	RestartAnimation(RefusedAnim);
}

void UCardSlotWidget::RestartAnimation(UWidgetAnimation* Anim)
{
	if (!Anim)
	{
		return; // optional animation not authored in this WBP
	}

	if (IsAnimationPlaying(Anim))
	{
		StopAnimation(Anim);
	}
	PlayAnimation(Anim);
}