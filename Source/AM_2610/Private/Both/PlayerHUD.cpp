// Fill out your copyright notice in the Description page of Project Settings.


#include "Both/PlayerHUD.h"
#include "Components/ProgressBar.h"
#include "Cards/CardHandComponent.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Cards/CardSlotWidget.h"
#include "Cards/BaseCard.h"
#include "Actors/Spawner.h"

void UPlayerHUD::SetHealth(float Percent)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
	}
}

void UPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (Crosshair)
	{
		DynamicMaterial = Crosshair->GetDynamicMaterial();
	}

	// Prompt starts hidden until the player focuses something
	if (InteractPromptText)
	{
		InteractPromptText->SetVisibility(ESlateVisibility::Collapsed);
	}
	CardSlots.Reset();
	CardSlots.Add(CardSlot_0);
	CardSlots.Add(CardSlot_1);
	CardSlots.Add(CardSlot_2);
}

void UPlayerHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	FVector2D ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);

	FVector WorldLocation;
	FVector WorldDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PC, ScreenCenter, WorldLocation, WorldDirection))
	{
		return;
	}

	const float TraceDistance = 10000.0f;
	FVector TraceEnd = WorldLocation + (WorldDirection * TraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		QueryParams.AddIgnoredActor(OwningPawn);
	}

	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, WorldLocation, TraceEnd, ObjectParams, QueryParams);

	EndPoint = bHit ? Hit.Location : TraceEnd;

	bool bHitEnemy = bHit && Hit.GetActor() && (Hit.GetActor()->IsA<APawn>() || Hit.GetActor()->IsA<ASpawner>());

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(ColorName, bHitEnemy ? DangerColor : SafeColor);
	}
}

void UPlayerHUD::SetAmmo(float Current, float Max)
{
	if (CurrentAmmoText)
	{
		CurrentAmmoText->SetText(FText::AsNumber(Current));
	}

	if (MaxAmmoText)
	{
		MaxAmmoText->SetText(FText::AsNumber(Max));
	}
}

void UPlayerHUD::SetInteractPrompt(const FInteractPrompt& Prompt)
{
	if (!InteractPromptText)
	{
		return;
	}

	if (!Prompt.HasTarget())
	{
		InteractPromptText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FText Message = FText::Format(
		NSLOCTEXT("PlayerHUD", "InteractPrompt", "Press [{0}] to {1}"),
		GetKeyText(Prompt.Action),
		Prompt.Verb);

	InteractPromptText->SetText(Message);
	InteractPromptText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText UPlayerHUD::GetKeyText(const UInputAction* Action) const
{
	if (!Action)
	{
		return FText::FromString(TEXT("?"));
	}

	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			const TArray<FKey> Keys = InputSubsystem->QueryKeysMappedToAction(Action);

			// Prefer a keyboard/mouse key so the prompt doesn't say "Gamepad Face Button Left"
			for (const FKey& Key : Keys)
			{
				if (!Key.IsGamepadKey())
				{
					return Key.GetDisplayName();
				}
			}
			if (Keys.Num() > 0)
			{
				return Keys[0].GetDisplayName();
			}
		}
	}

	// Nothing mapped - show the asset name so the problem is obvious in PIE
	return FText::FromName(Action->GetFName());
}
void UPlayerHUD::SetCardSlot(int32 SlotIndex, UBaseCard* Card)
{
	if (CardSlots.IsValidIndex(SlotIndex) && CardSlots[SlotIndex])
	{
		CardSlots[SlotIndex]->SetCard(Card);
	}
}

void UPlayerHUD::CardUsed(int32 SlotIndex, UBaseCard* Card)
{
	if (CardSlots.IsValidIndex(SlotIndex) && CardSlots[SlotIndex])
	{
		CardSlots[SlotIndex]->PlayUsed(Card);
	}
}
void UPlayerHUD::CardRefused(int32 SlotIndex, UBaseCard* Card, ECardFeedback Reason)
{
	// HandFull passes INDEX_NONE, which fails IsValidIndex, so it's ignored here
	if (CardSlots.IsValidIndex(SlotIndex) && CardSlots[SlotIndex])
	{
		CardSlots[SlotIndex]->PlayRefused();
	}
}