// Fill out your copyright notice in the Description page of Project Settings.


#include "Utility/Interactable.h"

bool IInteractable::CanInteract_Implementation(AActor* Interactor)
{
	return true;
}

FInteractPrompt IInteractable::GetInteractPrompt_Implementation(AActor* Interactor)
{
	FInteractPrompt Prompt;
	Prompt.Verb = FText::FromString(TEXT("Interact"));
	return Prompt;
}

void IInteractable::Interact_Implementation(AActor* Interactor)
{
	// Intentionally empty - implemented per interactable
}