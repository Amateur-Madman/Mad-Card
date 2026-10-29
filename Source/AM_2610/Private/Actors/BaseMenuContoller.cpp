// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BaseMenuContoller.h"
#include "Blueprint/UserWidget.h"

void ABaseMenuContoller::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the soft class reference (loads it now, only when actually needed)
	UClass* ResolvedMenuClass = MenuClass.LoadSynchronous();
	if (!ResolvedMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseMenuController::BeginPlay - failed to load MenuClass from soft reference"));
		return;
	}

	MenuObject = CreateWidget<UUserWidget>(this, ResolvedMenuClass);
	if (!MenuObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseMenuController::BeginPlay - failed to create MenuObject"));
		return;
	}

	MenuObject->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuObject->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(InputMode);

	FlushPressedKeys();
}