// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utility/Interactable.h"
#include "MazePipeConsole.generated.h"

class UBoxComponent;
class UAudioComponent;
class UMazePuzzleComponent;
class APawn;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM()
enum class EMazePipeConsoleFocus : uint8
{
    WaitingForPlayerQuery,
    PlayerUnavailable,
    NoPipeRoom,
    PuzzleNotReady,
    InvalidTarget,
    AimOffScreen,
    OutOfReach,
    ViewBlocked,
    Ready
};

// The existing player interaction system supplies the E input and HUD prompt.
UCLASS(Blueprintable)
class AM_2610_API AMazePipeConsole : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AMazePipeConsole();

    virtual bool CanInteract_Implementation(AActor* Interactor) override;
    virtual FInteractPrompt GetInteractPrompt_Implementation(AActor* Interactor) override;
    virtual void Interact_Implementation(AActor* Interactor) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe Console")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe Console")
    TObjectPtr<UStaticMeshComponent> ConsoleMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe Console")
    TObjectPtr<UBoxComponent> InteractionBox;

    // Registers a nearby console with the player's HUD. The screen rectangle decides aim.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe Console")
    TObjectPtr<USphereComponent> InteractionRange;

    // Maximum distance from the camera to the aimed point on the glass, in world centimeters.
    UPROPERTY(EditAnywhere, BlueprintSetter = SetMaxUseDistance, Category = "Pipe Console|Interaction", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxUseDistance = 120.0f;

    UFUNCTION(BlueprintSetter, Category = "Pipe Console|Interaction")
    void SetMaxUseDistance(float NewDistance);

    UPROPERTY(VisibleInstanceOnly, Transient, Category = "Pipe Console|Debug")
    EMazePipeConsoleFocus LastFocusCheck = EMazePipeConsoleFocus::WaitingForPlayerQuery;

    UPROPERTY(VisibleInstanceOnly, Transient, Category = "Pipe Console|Debug")
    TWeakObjectPtr<UPrimitiveComponent> LastBlockingComponent;

    // Assign the console test cue here. It plays only when a new steam check starts.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe Console|Audio")
    TObjectPtr<UAudioComponent> SteamTestAudio;

    // A child actor finds its room automatically. Set this only for a separate level actor.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Pipe Console")
    TObjectPtr<AActor> PuzzleRoom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Console")
    EInteractSlot InteractSlot = EInteractSlot::Primary;

    // The HUD supplies Press [E] to. Store only the action words here.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Console")
    FText InteractVerb = NSLOCTEXT("MazePipePuzzle", "TestPipesVerb", "Test Pipes");

    UFUNCTION(BlueprintPure, Category = "Pipe Console")
    UMazePuzzleComponent* GetPipePuzzle() const;

protected:
    virtual void PostInitializeComponents() override;

private:
    EMazePipeConsoleFocus EvaluateFocus(APawn* Player, FHitResult& Obstruction) const;
};