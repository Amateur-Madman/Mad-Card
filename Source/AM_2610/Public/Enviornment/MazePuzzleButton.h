// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MazePuzzleButton.generated.h"

class APawn;
class UBoxComponent;

UCLASS()
class AM_2610_API AMazePuzzleButton : public AActor
{
    GENERATED_BODY()

public:
    AMazePuzzleButton();

    // Adjust the inherited box in the button Blueprint to cover its standing area.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|Button")
    TObjectPtr<UBoxComponent> ButtonArea;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|Button")
    bool bIsPressed = false;

    // Use this event for the button's animation, material, or sound.
    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Button")
    void ButtonPressedChanged(bool bPressed);

    void GetOverlappingPawns(TArray<APawn*>& Pawns) const;
    void SetPressed(bool bPressed);
};
