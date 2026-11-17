// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AITargetStack.generated.h"

// Fired whenever the top of the stack changes (NewTarget is nullptr if the stack became empty)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAITargetChanged, AActor* /*NewTarget*/);

/**
 * "Who should enemies chase right now?" The most recently pushed target wins. The player pushes
 * itself at BeginPlay; a Decoy pushes itself on top and removes itself when it expires. Enemy
 * controllers subscribe to OnTargetChanged and update their Blackboard when the top changes.
 */
UCLASS()
class AM_2610_API UAITargetStack : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Puts Target on top of the stack (moves it to the top if it's already in there)
	UFUNCTION(BlueprintCallable, Category = "AI")
	void Push(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "AI")
	void Remove(AActor* Target);

	// Top of the stack, skipping anything that has been destroyed. nullptr if empty.
	UFUNCTION(BlueprintCallable, Category = "AI")
	AActor* GetCurrentTarget();

	// Controllers bind here instead of polling
	FOnAITargetChanged OnTargetChanged;

private:
	// Removes without broadcasting, so Push/Remove can compare old top vs new top themselves
	void RemoveInternal(AActor* Target);

	// Weak pointers so a destroyed decoy can never dangle
	TArray<TWeakObjectPtr<AActor>> Stack;
};
