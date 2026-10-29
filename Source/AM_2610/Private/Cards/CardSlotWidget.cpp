// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/CardSlotWidget.h"
#include "Cards/BaseCard.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UCardSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Also runs in the designer, so the key label and empty state preview correctly
	if (KeyText)
	{
		KeyText->SetText(KeyLabel);
		KeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (CardGhost)
	{
		CardGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
		CardGhost->SetRenderOpacity(0.f);
	}

	UsedElapsed = UsedDuration;
	// Remember the designer colors so the flash can return to them
	if (EmptyFrame) EmptyBaseColor = EmptyFrame->GetBrushColor();
	if (CardFrame) CardBaseColor = CardFrame->GetBrushColor();
	RefusedElapsed = RefusedDuration;
	ShowEmpty();
}

void UCardSlotWidget::ShowEmpty()
{
	if (EmptyFrame) EmptyFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (CardFrame) CardFrame->SetVisibility(ESlateVisibility::Collapsed);
	if (CardIcon) CardIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (NameText) NameText->SetVisibility(ESlateVisibility::Collapsed);

	bHasCard = false;

	// Cancel any pop-in in progress and put the whole slot back to normal
	GainElapsed = GainDuration;
	SetRenderScale(FVector2D(1.f, 1.f));
	SetRenderOpacity(1.f);
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
		GainElapsed = 0.f;
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
	CardGhost->SetRenderScale(FVector2D(1.f, 1.f));
	CardGhost->SetRenderOpacity(1.f);
	UsedElapsed = 0.f;
}

void UCardSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Pop-in: fade 0 -> 1 and scale 0.7 -> 1.08 -> 1.0 on the whole slot
	if (GainElapsed < GainDuration)
	{
		GainElapsed = FMath::Min(GainElapsed + InDeltaTime, GainDuration);
		const float T = GainElapsed / GainDuration;
		constexpr float PopPoint = 0.6f;

		const float Scale = (T < PopPoint)
			? FMath::Lerp(0.7f, 1.08f, T / PopPoint)
			: FMath::Lerp(1.08f, 1.0f, (T - PopPoint) / (1.f - PopPoint));

		SetRenderScale(FVector2D(Scale, Scale));
		SetRenderOpacity(FMath::Clamp(T / PopPoint, 0.f, 1.f));
	}

	// Used flash: the ghost grows and fades out, independent of the slot clearing
	if (UsedElapsed < UsedDuration)
	{
		UsedElapsed = FMath::Min(UsedElapsed + InDeltaTime, UsedDuration);
		const float T = UsedElapsed / UsedDuration;

		const float Scale = FMath::Lerp(1.f, GhostEndScale, T);
		CardGhost->SetRenderScale(FVector2D(Scale, Scale));
		CardGhost->SetRenderOpacity(1.f - T);
	}
	// Refused flash: snap to red, fade back, with a quick decaying shake
	if (RefusedElapsed < RefusedDuration)
	{
		RefusedElapsed = FMath::Min(RefusedElapsed + InDeltaTime, RefusedDuration);
		const float T = RefusedElapsed / RefusedDuration;
		const float Fade = 1.f - T;
		ApplyRefused(Fade, FMath::Sin(T * PI * 6.f) * RefusedShake * Fade);
	}
}
void UCardSlotWidget::PlayRefused()
{
	RefusedElapsed = 0.f;
}

void UCardSlotWidget::ApplyRefused(float Alpha, float ShakeX)
{
	if (EmptyFrame) EmptyFrame->SetBrushColor(FMath::Lerp(EmptyBaseColor, RefusedColor, Alpha));
	if (CardFrame) CardFrame->SetBrushColor(FMath::Lerp(CardBaseColor, RefusedColor, Alpha));
	SetRenderTranslation(FVector2D(ShakeX, 0.f));
}