// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WrittingBoard.h"
#include "Both/SentenceWordUnderlined.h"
#include "Components/WidgetComponent.h"

// Sets default values
AWrittingBoard::AWrittingBoard()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	CanvasComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("3DCanvasComponent"));
	CanvasComponent->SetWidgetSpace(EWidgetSpace::World);
	SetRootComponent(CanvasComponent);
	CanvasComponent->SetDrawSize(FVector2D(1920.0f, 1080.0f));
	CanvasComponent->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.2f));
}

// Called when the game starts or when spawned
void AWrittingBoard::BeginPlay()
{
	Super::BeginPlay();

	if (CanvasComponent)
	{
		CanvasComponent->SetWidgetClass(SentenceWordUnderlinedClass);
		CanvasComponent->InitWidget();

		UUserWidget* BaseWidget = CanvasComponent->GetUserWidgetObject();

		if (BaseWidget)
		{
			USentenceWordUnderlined* MainTextWidget = Cast<USentenceWordUnderlined>(BaseWidget);
			if (MainTextWidget)
			{
				MainTextWidget->SetupAccordingToDiff();
				MainTextWidget->ForceLayoutPrepass();
			}
		}
	}
}