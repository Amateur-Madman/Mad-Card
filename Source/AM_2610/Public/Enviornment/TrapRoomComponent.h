// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "TrapRoomComponent.generated.h"

class AMazeGenerator;
class APawn;
class UBoxComponent;
class UCapsuleComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class UShapeComponent;

UENUM(BlueprintType)
enum class ETrapRoomState : uint8
{
    Idle,
    Warning,
    Firing,
    Spent
};

// Add this to BP_TrapRoom without changing its BP_RoomBase parent.
UCLASS(ClassGroup = (Traps), meta = (BlueprintSpawnableComponent))
class AM_2610_API UTrapRoomComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrapRoomComponent();

    // Reuse the room's existing box to start the warning and check when it is empty.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Components")
    FName RoomAreaTag = TEXT("RoomArea");

    // Tag every damage box or capsule. Leave a safe gap near the doorway.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Components")
    FName DamageAreaTag = TEXT("TrapDamageArea");

    // Put this component tag on each Niagara component that shoots the flame.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Components")
    FName FlameTag = TEXT("TrapFlame");

    // Only tagged lights blink. Normal room lights keep their existing settings.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Components")
    FName WarningLightTag = TEXT("TrapWarningLight");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Components")
    FName WarningMeshTag = TEXT("TrapWarningMesh");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Timing", meta = (ClampMin = "0.0", Units = "s"))
    float WarningDuration = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Timing", meta = (ClampMin = "0.05", Units = "s"))
    float FlashInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Timing", meta = (ClampMin = "0.1", Units = "s"))
    float FireDuration = 3.0f;

    // Rearm after the fire ends and the player leaves. Disable for a single-use trap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Timing")
    bool bRepeatWhenEmpty = true;

    // These parameters belong to a copy of the bollard's emissive material.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Light")
    FName LightColorParameter = TEXT("TrapColor");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Light")
    FName LightStrengthParameter = TEXT("TrapGlow");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Light", meta = (ClampMin = "0.0"))
    float WarningGlow = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Light", meta = (ClampMin = "0.0"))
    float FireGlow = 12.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|State")
    ETrapRoomState TrapState = ETrapRoomState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|State")
    bool bTrapReady = false;

    UFUNCTION(BlueprintPure, Category = "Trap")
    bool IsPlayerInsideRoom(APawn* Player) const;

    UFUNCTION(BlueprintPure, Category = "Trap")
    bool IsPlayerInsideDamageArea(APawn* Player) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<UBoxComponent> RoomArea;
    TArray<TWeakObjectPtr<UShapeComponent>> DamageAreas;
    TWeakObjectPtr<AMazeGenerator> MazeGenerator;
    TArray<TWeakObjectPtr<UActorComponent>> FlameComponents;
    TArray<TWeakObjectPtr<ULightComponent>> WarningLights;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> WarningMaterials;

    FTimerHandle CheckTimer;
    FTimerHandle WarningTimer;
    FTimerHandle FlashTimer;
    FTimerHandle FireTimer;
    bool bLightOn = false;
    bool bLossReported = false;

    void InitializeTrap();
    void CheckPlayer();
    void StartWarning();
    void ToggleWarningLight();
    void StartFire();
    void FinishFire();
    void ResetTrap();
    void SetWarningLights(bool bOn);
    void SetFlames(bool bOn);
    void DestroyRoomCards();
    bool CanRunTrap() const;
    bool IsPlayerInsideBox(APawn* Player, const UBoxComponent* Box) const;
    bool IsPlayerInsideCapsule(APawn* Player, const UCapsuleComponent* Capsule) const;
};