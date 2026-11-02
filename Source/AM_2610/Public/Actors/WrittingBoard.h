// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/IInputProcessor.h"
#include "WrittingBoard.generated.h"

class FBoardInputInterceptor : public IInputProcessor
{
public:
	class AWrittingBoard* TargetBoard;

	FBoardInputInterceptor(class AWrittingBoard* InBoard) : TargetBoard(InBoard) {}
	virtual ~FBoardInputInterceptor() {}

	//virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	// This function intercepts text and character strokes globally
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FCharacterEvent& InCharacterEvent);
};

UCLASS()
class AM_2610_API AWrittingBoard : public AActor
{
	GENERATED_BODY()
	
public:	
	AWrittingBoard();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	class UBoxComponent* EditorOutline;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	class UWidgetComponent* CanvasComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TSubclassOf<UUserWidget> SentenceWordUnderlinedClass;

private:
	float DrawX = 1920.0f;
	float DrawY = 1080.0f;

	TSharedPtr<FBoardInputInterceptor> InputProcessor;
};
