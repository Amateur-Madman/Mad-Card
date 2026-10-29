// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AActor;
class UInputAction;

// Which context button an interactable wants. ABasePlayer maps each slot to an Input Action
// asset (Primary = IA_Interact1 on E, Secondary = IA_Interact2 on F in IMC_Player).
UENUM(BlueprintType)
enum class EInteractSlot : uint8
{
	Primary   UMETA(DisplayName = "Interact 1 (E)"),
	Secondary UMETA(DisplayName = "Interact 2 (F)")
};

// Payload broadcast by ABasePlayer::OnInteractPromptChanged.
// A null Target means "hide the prompt".
USTRUCT(BlueprintType)
struct FInteractPrompt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	EInteractSlot Slot = EInteractSlot::Primary;

	// Filled in by the player from Slot. The HUD asks the Enhanced Input subsystem
	// which key is currently mapped to it, so remapped keys show correctly.
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UInputAction> Action = nullptr;

	// The {} part of "Press [] to {}" - "Pick up", "Open", "Talk"...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Verb;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AActor> Target = nullptr;

	bool HasTarget() const { return Target != nullptr; }

	bool operator==(const FInteractPrompt& Other) const
	{
		return Slot == Other.Slot && Target == Other.Target && Verb.EqualTo(Other.Verb);
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class AM_2610_API IInteractable
{
	GENERATED_BODY()

public:
	// Is this interactable currently usable by Interactor? Return false to hide the prompt.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	bool CanInteract(AActor* Interactor);
	virtual bool CanInteract_Implementation(AActor* Interactor);

	// Which button and verb to show. Player fills in Action and Target.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	FInteractPrompt GetInteractPrompt(AActor* Interactor);
	virtual FInteractPrompt GetInteractPrompt_Implementation(AActor* Interactor);

	// The button was pressed while this interactable was focused.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor);
};