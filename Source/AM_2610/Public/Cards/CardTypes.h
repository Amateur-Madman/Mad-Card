// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CardTypes.generated.h"

// Why a card action was refused. Drives the negative-feedback sound and the HUD's refused flash.
UENUM(BlueprintType)
enum class ECardFeedback : uint8
{
	EmptySlot,   // tried to use a card with an empty hand (kept as EmptySlot so existing Blueprint references survive)
	CannotUse,   // the card's CanUse returned false
	HandFull     // tried to add a card with no free slot
};
