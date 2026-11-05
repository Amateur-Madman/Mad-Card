// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WrittingBoard.h"
#include "Both/SentenceWordUnderlined.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Actors/BasePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Actors/BaseGameMode.h"

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
		CanvasComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
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
			MainTextWidget = Cast<USentenceWordUnderlined>(BaseWidget);
			if (MainTextWidget)
			{
				MainTextWidget->SetupAccordingToDiff();
				MainTextWidget->OnCompleted.AddDynamic(this, &AWrittingBoard::HandleWin);
				MainTextWidget->OnFailed.AddDynamic(this, &AWrittingBoard::HandleLose);
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

void AWrittingBoard::HandleWin()
{
	if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->TriggerWin();
	}
}

void AWrittingBoard::HandleLose()
{
	if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->TriggerLose();
	}
}

void AWrittingBoard::SendKeyboardInput(const TCHAR& InKey)
{
	if (MainTextWidget)
	{
		if (MainTextWidget->CheckInput(InKey))
		{
			// Correct input
			OnRight.Broadcast();
		}
		else
		{
			// Incorrect input
			OnWrong.Broadcast();
		}
	}
}

void AWrittingBoard::AllowInputProcessing()
{
	CanProcessInput = true;
}

bool FBoardInputInterceptor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if(TargetBoard && TargetBoard->CanProcessInput && TargetBoard->PlayerReady)
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
		TargetBoard->CanProcessInput = false;
		TargetBoard->GetWorld()->GetTimerManager().SetTimer(
			TargetBoard->InputTimer,
			TargetBoard, 
			&AWrittingBoard::AllowInputProcessing, 
			TargetBoard->InputCooldown, 
			false);
	}
	return false;
}