// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/IInputProcessor.h"
#include "WrittingBoard.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInput);

class FBoardInputInterceptor : public IInputProcessor
{
public:
	class AWrittingBoard* TargetBoard;

	FBoardInputInterceptor(class AWrittingBoard* InBoard) : TargetBoard(InBoard) {}
	virtual ~FBoardInputInterceptor() {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	// This function intercepts text and character strokes globally
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
};

UCLASS()
class AM_2610_API AWrittingBoard : public AActor
{
	GENERATED_BODY()
	
public:	
	AWrittingBoard();

	void SendKeyboardInput(const TCHAR& InKey);

	FOnInput OnRight;
	FOnInput OnWrong;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	class UBoxComponent* EditorOutline;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	class UWidgetComponent* CanvasComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TSubclassOf<UUserWidget> SentenceWordUnderlinedClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	class USentenceWordUnderlined* MainTextWidget;

private:
	float DrawX = 1920.0f;
	float DrawY = 1080.0f;

	TSharedPtr<FBoardInputInterceptor> InputProcessor;

	UFUNCTION()
	void HandleWin();

	UFUNCTION()
	void HandleLose();
};
