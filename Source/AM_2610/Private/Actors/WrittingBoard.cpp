// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WrittingBoard.h"
#include "Both/SentenceWordUnderlined.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Actors/BasePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Sets default values
AWrittingBoard::AWrittingBoard()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	CanvasComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("3DCanvasComponent"));
	if(CanvasComponent)
	{
		CanvasComponent->SetWidgetSpace(EWidgetSpace::World);
		SetRootComponent(CanvasComponent);
		CanvasComponent->SetDrawSize(FVector2D(DrawX, DrawY));
		CanvasComponent->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
	}

	float HalfWidth = DrawX / 2.0f;
	float HalfHeight = DrawY / 2.0f;

	EditorOutline = CreateDefaultSubobject<UBoxComponent>(TEXT("EditorOutline"));
	if (EditorOutline)
	{
		EditorOutline->SetupAttachment(RootComponent);
		EditorOutline->SetBoxExtent(FVector(2.0f, HalfWidth, HalfHeight)); 
		EditorOutline->SetHiddenInGame(true);
		EditorOutline->SetLineThickness(4.0f);
		EditorOutline->ShapeColor = FColor::Yellow; 
	}
}

void AWrittingBoard::SendKeyboardInput(const TCHAR& InKey)
{
	if (CanvasComponent)
	{
		UUserWidget* BaseWidget = CanvasComponent->GetUserWidgetObject();
		if (BaseWidget)
		{
			USentenceWordUnderlined* MainTextWidget = Cast<USentenceWordUnderlined>(BaseWidget);
			if (MainTextWidget)
			{
				MainTextWidget->CheckInput(InKey);
			}
		}
	}
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
			}
		}

		if (FSlateApplication::IsInitialized())
		{
			InputProcessor = MakeShareable(new FBoardInputInterceptor(this));
			FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
		}
	}
}

void AWrittingBoard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Disposes layout cleanly when the scene closes out or changes
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		InputProcessor.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

bool FBoardInputInterceptor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if(TargetBoard)
	{
		FKey PressedKey = InKeyEvent.GetKey();
		FString KeyName = PressedKey.ToString();
		if (KeyName.Len() == 1)
		{
			TCHAR Letter = KeyName[0];
			if (PressedKey == EKeys::W || PressedKey == EKeys::A || PressedKey == EKeys::S || PressedKey == EKeys::D)
			{
				TargetBoard->SendKeyboardInput(Letter);
				return false;
			}
			TargetBoard->SendKeyboardInput(Letter);
		}
		//return true;
	}
	return false;
}