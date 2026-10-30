// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WrittingBoard.h"
#include "Both/SentenceWordUnderlined.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"

// Sets default values
AWrittingBoard::AWrittingBoard()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Board = CreateDefaultSubobject<UStaticMeshComponent>("Board");
	if (Board) SetRootComponent(Board);

	CanvasComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("3DCanvasComponent"));
	CanvasComponent->SetWidgetSpace(EWidgetSpace::World);
	CanvasComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AWrittingBoard::BeginPlay()
{
	Super::BeginPlay();

	MatchCanvasSize();

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
	}
}

void AWrittingBoard::MatchCanvasSize()
{
    if (!Board || !CanvasComponent) return;

    UStaticMesh* MeshAsset = Board->GetStaticMesh();
    if (!MeshAsset) return;

    FBox LocalBounds = MeshAsset->GetBoundingBox();
    FVector MeshLocalSize = LocalBounds.GetSize();

    FVector ActorScale = Board->GetComponentScale();
    FVector MeshWorldSize = MeshLocalSize * ActorScale;

    float TargetWidth = MeshWorldSize.Y;
    float TargetHeight = MeshWorldSize.Z;

	CanvasComponent->SetDrawSize(FVector2D(TargetWidth, TargetHeight));
}
