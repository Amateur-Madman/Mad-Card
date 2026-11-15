// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enviornment/MazePipeFitting.h"
#include "MazePipePiece.generated.h"

class UArrowComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

USTRUCT()
struct AM_2610_API FMazePipeSurfaceMaterials
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> Materials;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> OverlayMaterial;

    UPROPERTY(Transient)
    bool bForceDisableNanite = false;
};

// Create pipe mesh Blueprints from this Actor. Keep MazePipePuzzle as a plain helper.
UCLASS(Blueprintable)
class AM_2610_API AMazePipePiece : public AActor
{
    GENERATED_BODY()

public:
    AMazePipePiece();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    TObjectPtr<UStaticMeshComponent> PipeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Connectors")
    TObjectPtr<UArrowComponent> PortUp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Connectors")
    TObjectPtr<UArrowComponent> PortRight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Connectors")
    TObjectPtr<UArrowComponent> PortDown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Connectors")
    TObjectPtr<UArrowComponent> PortLeft;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UStaticMeshComponent> FlowIndicator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UStaticMeshComponent> RequiredIndicator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UPointLightComponent> FlowLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UPointLightComponent> RequiredLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UTextRenderComponent> RequiredLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Lights")
    TObjectPtr<UTextRenderComponent> EndpointLabel;

    // An Unlit material with IndicatorColor and IndicatorGlow parameters. Additive preserves the pipe paint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    TObjectPtr<UMaterialInterface> IndicatorMaterial;

    // Apply the shared light material to the fitted body and every connector sleeve.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    bool bGlowWholePipe = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights", meta = (ClampMin = "0.0", ClampMax = "20.0"))
    float PipeFlowGlow = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor SteamLightColor = FLinearColor(0.0f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor InletLightColor = FLinearColor(0.0f, 1.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor OutletLightColor = FLinearColor(0.0f, 1.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor RequiredLightColor = FLinearColor(1.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor ConnectedLightColor = FLinearColor(0.0f, 1.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FLinearColor LeakLightColor = FLinearColor(1.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights", meta = (ClampMin = "0.0", ClampMax = "20.0"))
    float IndicatorGlow = 3.0f;

    // Optional local spill. Sphere mode also uses this when the indicator material is missing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    bool bUsePointLights = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights", meta = (ClampMin = "0.0"))
    float IndicatorLightIntensity = 20.0f;

    // Offsets are multiples of target pipe diameter in the fitted Actor axes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FVector FlowIndicatorOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    FVector RequiredIndicatorOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    bool bShowChamberLabel = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Lights")
    bool bShowEndpointLabels = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Pipe|State")
    EMazePipeFlowLightState FlowLightState = EMazePipeFlowLightState::Idle;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Pipe|State")
    EMazePipeChamberLightState ChamberLightState = EMazePipeChamberLightState::Required;

    // Source meshes are arranged flat in XY, with their front facing positive Z.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Fitting", meta = (ValidEnumValues = "Straight,Bend,Tee,Cross,Chamber", ToolTip = "Choose the source mesh shape. Inlet and outlet roles are assigned by the puzzle."))
    EMazePipePiece CalibrationPiece = EMazePipePiece::Straight;

    // Bounds provide a starting estimate. Disable this to place the arrows precisely.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Fitting")
    bool bAutoEstimateConnectors = true;

    // Zero uses the straight source mesh diameter. The straight source measures its own bounds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Fitting", meta = (ClampMin = "0.0", Units = "cm"))
    float NativePipeDiameter = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Pipe|State")
    int32 GridCellIndex = INDEX_NONE;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Pipe|State")
    bool bRightWall = false;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Fitting", meta = (DisplayName = "Estimate Connector Positions", ToolTip = "Estimate connector arrow positions from the source mesh bounds. This replaces the current arrow positions."))
    void EstimateConnectorPositions();

    UFUNCTION(BlueprintCallable, Category = "Pipe|Lights")
    void SetIndicatorStates(EMazePipeFlowLightState FlowState, EMazePipeChamberLightState ChamberState);

    bool ReadSourceGeometry(float ReferenceDiameter, FMazePipeSourceGeometry& OutGeometry, FString& OutError) const;
    bool FitToCell(const FMazePipeCell& Cell, float CellSize, float TargetDiameter, float WallReach,
        const AMazePipePiece* StraightSource, FString& OutError);
    void ConfigureIndicators(const FMazePipeCell& Cell, const FMazePipeGridFrame& Frame);

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    bool GetEstimatedPorts(TArray<FVector>& OutPositions, FString& OutError, float ReferenceDiameter = 0.0f) const;
    float GetSourceDiameter(float ReferenceDiameter) const;
    FTransform GetAuthoredMeshTransform() const;
    void ApplyEstimatedPorts(const TArray<FVector>& Positions);
    void ClearSleeves();
    void ApplyIndicator(UStaticMeshComponent* Mesh, UPointLightComponent* Light,
        UMaterialInstanceDynamic* Material, const FLinearColor& Color, bool bLit);
    void CachePipeSurfaceMaterials();
    void RestorePipeSurfaceMaterials();
    void ApplyPipeSurfaceGlow(const FLinearColor& Color, bool bLit);

    UPROPERTY(Transient)
    TArray<FMazePipeSurfaceMaterials> PipeSurfaceMaterials;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PipeSurfaceGlowMaterial;

    bool bSurfaceUsesOverlay = false;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FlowIndicatorMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RequiredIndicatorMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> SleeveMeshes;

    FTransform AuthoredMeshTransform = FTransform::Identity;
    bool bCapturedMeshTransform = false;
    bool bFitted = false;
    bool bRequiredChamber = false;
    EMazePipePiece FittedPiece = EMazePipePiece::Empty;
    FVector FittedIndicatorCenter = FVector::ZeroVector;
    float FittedPipeDiameter = 0.0f;
};