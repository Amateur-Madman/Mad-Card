// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MazePipeInteractionComponent.generated.h"

class AMazePipePiece;
class APawn;
class UInputAction;
class FMazePipeInputState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMazePipeFocusChanged, AMazePipePiece*, Pipe, bool, bCanRotate);

// Add one component to the player Blueprint and assign its existing E and F actions.
UCLASS(ClassGroup = (Maze), meta = (BlueprintSpawnableComponent))
class AM_2610_API UMazePipeInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMazePipeInteractionComponent();
    virtual void Deactivate() override;

    FText GetRotationPromptVerb() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Interaction")
    TObjectPtr<UInputAction> RotateLeftAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Interaction")
    TObjectPtr<UInputAction> RotateRightAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Interaction", meta = (ClampMin = "1.0", ClampMax = "1000.0", Units = "cm"))
    float InteractionDistance = 300.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Pipe Interaction|State")
    TObjectPtr<AMazePipePiece> FocusedPipe;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Pipe Interaction|State")
    bool bCanRotateFocusedPipe = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Pipe Interaction|State")
    bool bInputReady = false;

    // Optional focus notification. Pipe prompts use the existing player HUD.
    UPROPERTY(BlueprintAssignable, Category = "Pipe Interaction|Events")
    FOnMazePipeFocusChanged OnPipeFocusChanged;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    bool PrepareInput(APawn* Player);
    bool HandlePipeInput(bool bRotateRight);
    AMazePipePiece* TracePipe(APawn* Player) const;
    void SetFocusedPipe(AMazePipePiece* Pipe);
    void ReleaseInput();

    TSharedPtr<FMazePipeInputState> InputRouting;
    TWeakObjectPtr<UInputAction> BoundLeftAction;
    TWeakObjectPtr<UInputAction> BoundRightAction;
    float InputWaitSeconds = 0.0f;
    bool bWarnedInput = false;
};