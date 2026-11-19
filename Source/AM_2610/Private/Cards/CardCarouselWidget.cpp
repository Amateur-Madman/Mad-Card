// Fill out your copyright notice in the Description page of Project Settings.


#include "Cards/CardCarouselWidget.h"
#include "Kismet/GameplayStatics.h"

void UCardCarouselWidget::BindToHand(UCardHandComponent* InHand)
{
	UnbindFromHand();

	Hand = InHand;
	if (InHand)
	{
		InHand->OnHandChanged.AddDynamic(this, &UCardCarouselWidget::HandleHandChanged);
		InHand->OnCardSelectionChanged.AddDynamic(this, &UCardCarouselWidget::HandleSelectionChanged);
		InHand->OnCardCycled.AddDynamic(this, &UCardCarouselWidget::HandleCardCycled);
		InHand->OnCardUsed.AddDynamic(this, &UCardCarouselWidget::HandleCardUsed);
		InHand->OnCardRefused.AddDynamic(this, &UCardCarouselWidget::HandleCardRefused);
	}

	// Starting cards were added before this widget existed
	Redraw();
}

void UCardCarouselWidget::NativeDestruct()
{
	UnbindFromHand();
	Super::NativeDestruct();
}

void UCardCarouselWidget::UnbindFromHand()
{
	if (UCardHandComponent* BoundHand = Hand.Get())
	{
		BoundHand->OnHandChanged.RemoveDynamic(this, &UCardCarouselWidget::HandleHandChanged);
		BoundHand->OnCardSelectionChanged.RemoveDynamic(this, &UCardCarouselWidget::HandleSelectionChanged);
		BoundHand->OnCardCycled.RemoveDynamic(this, &UCardCarouselWidget::HandleCardCycled);
		BoundHand->OnCardUsed.RemoveDynamic(this, &UCardCarouselWidget::HandleCardUsed);
		BoundHand->OnCardRefused.RemoveDynamic(this, &UCardCarouselWidget::HandleCardRefused);
	}
	Hand.Reset();
}

void UCardCarouselWidget::HandleHandChanged()
{
	Redraw();
}

void UCardCarouselWidget::HandleSelectionChanged(int32 SelectedIndex, UBaseCard* Card, int32 Direction)
{
	// Contents first, then the slide, so the row appears to carry the new cards in
	Redraw();

	if (Direction > 0)
	{
		RestartAnimation(SlideNextAnim);
	}
	else if (Direction < 0)
	{
		RestartAnimation(SlidePrevAnim);
	}
}

void UCardCarouselWidget::HandleCardCycled(int32 Direction)
{
	// Manual cycling only - the auto-advance after a use does not fire OnCardCycled, so it never
	// stacks a cycle sound on top of the card's own use sound
	USoundBase* Sound = (Direction < 0 && CyclePrevSound) ? CyclePrevSound.Get() : CycleNextSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, CycleVolume);
	}
}

void UCardCarouselWidget::HandleCardUsed(int32 SlotIndex, UBaseCard* Card)
{
	// Fires before the hand removes the card, so the ghost of the spent card flies out of the
	// center slot while the next card is already sliding in behind it
	CurrentSlot->PlayUsed(Card);
}

void UCardCarouselWidget::HandleCardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason)
{
	// HandFull is about a pickup, not the selected card
	if (Reason == ECardFeedback::HandFull)
	{
		return;
	}

	CurrentSlot->PlayRefused();
}

void UCardCarouselWidget::Redraw()
{
	const UCardHandComponent* BoundHand = Hand.Get();
	const int32 Count = BoundHand ? BoundHand->GetCardCount() : 0;

	// With an empty hand the current slot shows its "nothing here" outline
	CurrentSlot->SetCard(BoundHand ? BoundHand->GetSelectedCard() : nullptr);

	// Neighbors only exist when there is somewhere to cycle to. With exactly two cards the previous
	// and next card are the same card, so only Next is shown. Hidden (not Collapsed) keeps the
	// layout, so the current card never moves.
	const bool bShowPrev = Count > 2;
	const bool bShowNext = Count > 1;

	PrevSlot->SetCard(bShowPrev ? BoundHand->GetCardRelativeToSelection(-1) : nullptr);
	NextSlot->SetCard(bShowNext ? BoundHand->GetCardRelativeToSelection(+1) : nullptr);
	PrevSlot->SetVisibility(bShowPrev ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	NextSlot->SetVisibility(bShowNext ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	if (CounterText)
	{
		CounterText->SetText(Count > 1
			? FText::Format(NSLOCTEXT("CardCarousel", "Counter", "{0}/{1}"), BoundHand->GetSelectedIndex() + 1, Count)
			: FText::GetEmpty());
	}
}

void UCardCarouselWidget::RestartAnimation(UWidgetAnimation* Anim)
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
