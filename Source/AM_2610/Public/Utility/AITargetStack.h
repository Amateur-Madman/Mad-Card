// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AITargetStack.generated.h"

/**
 * "Who should enemies chase right now?" The most recently pushed target wins. The player pushes
 * itself at BeginPlay; a Decoy pushes itself on top and removes itself when it expires, so the
 * enemy's AIController just asks GetCurrentTarget().
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

private:
	// Weak pointers so a destroyed decoy can never dangle
	TArray<TWeakObjectPtr<AActor>> Stack;
};
