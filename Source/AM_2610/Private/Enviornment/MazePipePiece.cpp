// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipePiece.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

AMazePipePiece::AMazePipePiece()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Movable);

    PipeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PipeMesh"));
    PipeMesh->SetupAttachment(SceneRoot);
    PipeMesh->SetMobility(EComponentMobility::Movable);
    PipeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PipeMesh->SetCanEverAffectNavigation(false);

    FlowIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlowIndicator"));
    RequiredIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RequiredIndicator"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> IndicatorSphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (UStaticMeshComponent* Indicator : { FlowIndicator.Get(), RequiredIndicator.Get() })
    {
        Indicator->SetupAttachment(SceneRoot);
        Indicator->SetMobility(EComponentMobility::Movable);
        Indicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Indicator->SetCanEverAffectNavigation(false);
        Indicator->SetCastShadow(false);
        Indicator->SetVisibility(false);
        if (IndicatorSphere.Succeeded())
        {
            Indicator->SetStaticMesh(IndicatorSphere.Object);
        }
    }

    FlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlowLight"));
    RequiredLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RequiredLight"));
    for (UPointLightComponent* Light : { FlowLight.Get(), RequiredLight.Get() })
    {
        Light->SetupAttachment(SceneRoot);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetCastShadows(false);
        Light->bUseInverseSquaredFalloff = false;
        Light->SetVisibility(false);
    }

    RequiredLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RequiredLabel"));
    EndpointLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EndpointLabel"));
    for (UTextRenderComponent* Label : { RequiredLabel.Get(), EndpointLabel.Get() })
    {
        Label->SetupAttachment(SceneRoot);
        Label->SetMobility(EComponentMobility::Movable);
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Label->SetCanEverAffectNavigation(false);
        Label->SetCastShadow(false);
        Label->SetTextRenderColor(FColor::Black);
        Label->SetVisibility(false);
    }

    PortUp = CreateDefaultSubobject<UArrowComponent>(TEXT("PortUp"));
    PortRight = CreateDefaultSubobject<UArrowComponent>(TEXT("PortRight"));
    PortDown = CreateDefaultSubobject<UArrowComponent>(TEXT("PortDown"));
    PortLeft = CreateDefaultSubobject<UArrowComponent>(TEXT("PortLeft"));
    const TArray<UArrowComponent*> Arrows = { PortUp, PortRight, PortDown, PortLeft };
    const FVector Positions[] = { FVector(0, 50, 0), FVector(50, 0, 0), FVector(0, -50, 0), FVector(-50, 0, 0) };
    const float Yaw[] = { 90.0f, 0.0f, -90.0f, 180.0f };
    for (int32 Index = 0; Index < Arrows.Num(); ++Index)
    {
        Arrows[Index]->SetupAttachment(SceneRoot);
        Arrows[Index]->SetMobility(EComponentMobility::Movable);
        Arrows[Index]->SetRelativeLocation(Positions[Index]);
        Arrows[Index]->SetRelativeRotation(FRotator(0.0f, Yaw[Index], 0.0f));
        Arrows[Index]->SetHiddenInGame(true);
        Arrows[Index]->ArrowSize = 0.4f;
    }
}

void AMazePipePiece::BeginPlay()
{
    Super::BeginPlay();
    // Remove an editor preview if the level duplicates it into Play.
    if (Tags.Contains(FName(TEXT("MazePipeMeshPreview"))))
    {
        Destroy();
    }
}

FTransform AMazePipePiece::GetAuthoredMeshTransform() const
{
    return bCapturedMeshTransform ? AuthoredMeshTransform : PipeMesh->GetRelativeTransform();
}

float AMazePipePiece::GetSourceDiameter(float ReferenceDiameter) const
{
    if (NativePipeDiameter > 0.0f)
    {
        return NativePipeDiameter;
    }
    if (CalibrationPiece != EMazePipePiece::Straight && ReferenceDiameter > 0.0f)
    {
        return ReferenceDiameter;
    }
    if (!IsValid(PipeMesh) || !IsValid(PipeMesh->GetStaticMesh()))
    {
        return 0.0f;
    }
    const FVector Size = PipeMesh->GetStaticMesh()->GetBoundingBox().TransformBy(GetAuthoredMeshTransform()).GetSize();
    return static_cast<float>(CalibrationPiece == EMazePipePiece::Straight ? FMath::Max(Size.X, Size.Z) : Size.Z);
}

bool AMazePipePiece::GetEstimatedPorts(TArray<FVector>& OutPositions, FString& OutError, float ReferenceDiameter) const
{
    OutPositions.Reset();
    if (!IsValid(PipeMesh) || !IsValid(PipeMesh->GetStaticMesh()))
    {
        OutError = TEXT("Assign a static mesh to the PipeMesh component in the pipe Blueprint.");
        return false;
    }
    const FBox Bounds = PipeMesh->GetStaticMesh()->GetBoundingBox().TransformBy(GetAuthoredMeshTransform());
    const FVector Size = Bounds.GetSize();
    const FVector Center = Bounds.GetCenter();
    if (Size.X <= 0.01 || Size.Y <= 0.01 || Size.Z <= 0.01)
    {
        OutError = TEXT("The pipe source mesh has invalid bounds.");
        return false;
    }
    if (CalibrationPiece == EMazePipePiece::Straight && Size.Y <= FMath::Max(Size.X, Size.Z) * 1.01)
    {
        OutError = TEXT("Rotate the straight PipeMesh component so its long axis follows the local Y axis.");
        return false;
    }
    if ((CalibrationPiece == EMazePipePiece::Bend || CalibrationPiece == EMazePipePiece::Tee || CalibrationPiece == EMazePipePiece::Cross)
        && Size.Z >= FMath::Min(Size.X, Size.Y))
    {
        OutError = TEXT("Rotate the pipe mesh flat into the local XY plane, with its front facing positive Z.");
        return false;
    }

    const float Radius = GetSourceDiameter(ReferenceDiameter) * 0.5f;
    FVector Joint = Center;
    if (CalibrationPiece == EMazePipePiece::Bend)
    {
        Joint.X = Bounds.Min.X + Radius;
        Joint.Y = Bounds.Min.Y + Radius;
    }
    else if (CalibrationPiece == EMazePipePiece::Tee)
    {
        Joint.Y = Bounds.Min.Y + Radius;
    }
    else if (CalibrationPiece == EMazePipePiece::Chamber)
    {
        Joint.Z = Bounds.Min.Z + Radius;
    }

    OutPositions = { FVector(Joint.X, Bounds.Max.Y, Joint.Z), FVector(Bounds.Max.X, Joint.Y, Joint.Z),
        FVector(Joint.X, Bounds.Min.Y, Joint.Z), FVector(Bounds.Min.X, Joint.Y, Joint.Z) };
    return true;
}

void AMazePipePiece::ApplyEstimatedPorts(const TArray<FVector>& Positions)
{
    const TArray<UArrowComponent*> Arrows = { PortUp, PortRight, PortDown, PortLeft };
    if (Positions.Num() == Arrows.Num())
    {
        for (int32 Index = 0; Index < Arrows.Num(); ++Index)
        {
            Arrows[Index]->SetRelativeLocation(Positions[Index]);
        }
    }
}

void AMazePipePiece::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!bFitted)
    {
        FMazePipeCell Cell;
        Cell.Piece = CalibrationPiece;
        const uint8 Ports = FMazePipePuzzle::GetPorts(Cell);
        const TArray<UArrowComponent*> Arrows = { PortUp, PortRight, PortDown, PortLeft };
        for (int32 Index = 0; Index < Arrows.Num(); ++Index)
        {
            Arrows[Index]->SetVisibility((Ports & (1 << Index)) != 0);
        }
    }
    if (bAutoEstimateConnectors && !bFitted)
    {
        TArray<FVector> Positions;
        FString Error;
        if (GetEstimatedPorts(Positions, Error))
        {
            ApplyEstimatedPorts(Positions);
        }
    }
}

void AMazePipePiece::EstimateConnectorPositions()
{
    if (bFitted)
    {
        return;
    }
    TArray<FVector> Positions;
    FString Error;
    if (!GetEstimatedPorts(Positions, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("Pipe source: %s"), *Error);
        return;
    }
    ApplyEstimatedPorts(Positions);
}

bool AMazePipePiece::ReadSourceGeometry(float ReferenceDiameter, FMazePipeSourceGeometry& OutGeometry, FString& OutError) const
{
    if (!IsValid(PipeMesh) || !IsValid(PipeMesh->GetStaticMesh()))
    {
        OutError = TEXT("Assign a static mesh to the PipeMesh component in the pipe Blueprint.");
        return false;
    }
    TArray<FVector> Positions;
    if (bAutoEstimateConnectors)
    {
        if (!GetEstimatedPorts(Positions, OutError, ReferenceDiameter))
        {
            return false;
        }
    }
    else
    {
        Positions = { PortUp->GetRelativeLocation(), PortRight->GetRelativeLocation(), PortDown->GetRelativeLocation(), PortLeft->GetRelativeLocation() };
    }
    return FMazePipeFitting::ReadSource(CalibrationPiece, Positions, GetSourceDiameter(ReferenceDiameter), OutGeometry, OutError);
}

void AMazePipePiece::ClearSleeves()
{
    RestorePipeSurfaceMaterials();
    PipeSurfaceMaterials.Reset();
    PipeSurfaceGlowMaterial = nullptr;
    for (UStaticMeshComponent* Sleeve : SleeveMeshes)
    {
        if (IsValid(Sleeve))
        {
            Sleeve->DestroyComponent();
        }
    }
    SleeveMeshes.Reset();
}

bool AMazePipePiece::FitToCell(const FMazePipeCell& Cell, float CellSize, float TargetDiameter, float WallReach,
    const AMazePipePiece* StraightSource, FString& OutError)
{
    const bool bWallConnection = Cell.Piece == EMazePipePiece::Inlet || Cell.Piece == EMazePipePiece::Outlet;
    const EMazePipePiece Expected = bWallConnection ? EMazePipePiece::Bend : Cell.Piece;
    if (CalibrationPiece != Expected || !IsValid(StraightSource) || StraightSource->CalibrationPiece != EMazePipePiece::Straight)
    {
        OutError = TEXT("The pipe Blueprint shape must match its class slot. Inlet and outlet connections reuse the Bend source.");
        return false;
    }
    if (!bCapturedMeshTransform)
    {
        AuthoredMeshTransform = PipeMesh->GetRelativeTransform();
        bCapturedMeshTransform = true;
    }

    FMazePipeSourceGeometry StraightGeometry;
    FMazePipeSourceGeometry Geometry;
    if (!StraightSource->ReadSourceGeometry(0.0f, StraightGeometry, OutError)
        || !ReadSourceGeometry(StraightGeometry.Diameter, Geometry, OutError))
    {
        return false;
    }
    FTransform BodyTransform;
    TArray<FMazePipeSleeve> Sleeves;
    if (!FMazePipeFitting::FitMesh(AuthoredMeshTransform, Geometry, TargetDiameter, CellSize, BodyTransform, OutError)
        || !FMazePipeFitting::BuildSleeves(Geometry, CellSize, TargetDiameter, bWallConnection, WallReach, Sleeves, OutError))
    {
        return false;
    }
    TArray<FTransform> SleeveTransforms;
    for (const FMazePipeSleeve& Sleeve : Sleeves)
    {
        FTransform Transform;
        if (!FMazePipeFitting::FitSleeve(StraightSource->GetAuthoredMeshTransform(), StraightGeometry, Sleeve, TargetDiameter, Transform, OutError))
        {
            return false;
        }
        SleeveTransforms.Add(Transform);
    }

    ClearSleeves();
    PipeMesh->SetRelativeTransform(BodyTransform);
    for (const FTransform& Transform : SleeveTransforms)
    {
        UStaticMeshComponent* Sleeve = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
        Sleeve->SetupAttachment(SceneRoot);
        Sleeve->SetMobility(EComponentMobility::Movable);
        Sleeve->SetStaticMesh(StraightSource->PipeMesh->GetStaticMesh());
        Sleeve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sleeve->SetCanEverAffectNavigation(false);
        Sleeve->SetRelativeTransform(Transform);
        for (int32 Material = 0; Material < StraightSource->PipeMesh->GetNumMaterials(); ++Material)
        {
            Sleeve->SetMaterial(Material, StraightSource->PipeMesh->GetMaterial(Material));
        }
        AddInstanceComponent(Sleeve);
        Sleeve->RegisterComponent();
        SleeveMeshes.Add(Sleeve);
    }
    PortUp->SetVisibility(false);
    PortRight->SetVisibility(false);
    PortDown->SetVisibility(false);
    PortLeft->SetVisibility(false);
    FittedIndicatorCenter = Geometry.Piece == EMazePipePiece::Bend
        ? FVector(Geometry.PortDistances[1], Geometry.PortDistances[0], 0) * (0.292893218 * TargetDiameter / Geometry.Diameter)
        : FVector::ZeroVector;
    if (bWallConnection)
    {
        const double BoardReach = Geometry.PortDistances[0] * TargetDiameter / Geometry.Diameter;
        FittedIndicatorCenter = FVector(0, (BoardReach + CellSize * 0.5f) * 0.5, 0);
    }
    FittedPipeDiameter = TargetDiameter;
    bFitted = true;
    return true;
}

void AMazePipePiece::ConfigureIndicators(const FMazePipeCell& Cell, const FMazePipeGridFrame& Frame)
{
    FittedPiece = Cell.Piece;
    bRequiredChamber = Cell.Piece == EMazePipePiece::Chamber;
    // The fitted tube diameter is shared by all preview and runtime pieces.
    const float IndicatorDiameter = FMath::Max(1.0f, FittedPipeDiameter);
    const FVector Facing = GetActorTransform().InverseTransformVectorNoScale(Frame.Inward).GetSafeNormal();
    const FVector Up = GetActorTransform().InverseTransformVectorNoScale(Frame.Up).GetSafeNormal();
    FVector Mount = FittedIndicatorCenter;
    const bool bWallConnection = Cell.Piece == EMazePipePiece::Inlet || Cell.Piece == EMazePipePiece::Outlet;
    if (bWallConnection)
    {
        Mount += Facing * (IndicatorDiameter * 0.6f);
    }
    else
    {
        const FBox Bounds = PipeMesh->GetStaticMesh()->GetBoundingBox().TransformBy(PipeMesh->GetRelativeTransform());
        Mount.Z = Facing.Z >= 0.0 ? Bounds.Max.Z : Bounds.Min.Z;
        Mount += Facing * (IndicatorDiameter * 0.12f);
    }
    const FVector FlowMount = (bRequiredChamber ? FVector(0, -IndicatorDiameter * 1.2f, 0)
        + Facing * (IndicatorDiameter * 0.6f) : Mount) + FlowIndicatorOffset * IndicatorDiameter;
    const FVector RequiredMount = Mount + RequiredIndicatorOffset * IndicatorDiameter;
    const auto PlaceIndicator = [Facing](UStaticMeshComponent* Mesh, const FVector& Location, float Size)
        {
            const double NativeSize = IsValid(Mesh->GetStaticMesh()) ? Mesh->GetStaticMesh()->GetBoundingBox().GetSize().GetMax() : 100.0;
            Mesh->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromZ(Facing).ToQuat(), Location,
                FVector(Size / FMath::Max(0.01, NativeSize))));
        };
    PlaceIndicator(FlowIndicator, FlowMount, IndicatorDiameter * 0.18f);
    PlaceIndicator(RequiredIndicator, RequiredMount, IndicatorDiameter * 0.24f);
    FlowLight->SetRelativeLocation(FlowMount + Facing * IndicatorDiameter * 0.3f);
    RequiredLight->SetRelativeLocation(RequiredMount + Facing * IndicatorDiameter * 0.3f);
    for (UPointLightComponent* Light : { FlowLight.Get(), RequiredLight.Get() })
    {
        Light->SetAttenuationRadius(IndicatorDiameter * 3.0f);
        Light->SetIntensity(FMath::Max(0.0f, IndicatorLightIntensity));
    }
    const float LabelClearance = FMath::Max(0.05f, IndicatorDiameter * 0.01f);
    const FVector LabelMount = bWallConnection
        ? FittedIndicatorCenter + Facing * (IndicatorDiameter * 0.5f + LabelClearance)
        : Mount - Facing * (IndicatorDiameter * 0.12f - LabelClearance);
    const auto PlaceLabel = [Facing, Up, IndicatorDiameter](UTextRenderComponent* Label, const FVector& Location,
        float Width, float Height, const TCHAR* Text)
        {
            Label->SetRelativeLocation(Location);
            Label->SetRelativeRotation(FRotationMatrix::MakeFromXZ(Facing, Up).Rotator());
            Label->SetRelativeScale3D(FVector::OneVector);
            Label->SetWorldSize(IndicatorDiameter * 0.45f);
            Label->SetText(FText::FromString(Text));
            const FVector Size = Label->GetTextLocalSize();
            const double Scale = FMath::Min(1.0, FMath::Min(Width / FMath::Max(0.01, Size.Y), Height / FMath::Max(0.01, Size.Z)));
            Label->SetRelativeScale3D(FVector(Scale));
        };
    // Size the chamber label for its longest status so later changes remain on the pipe.
    PlaceLabel(RequiredLabel, LabelMount, IndicatorDiameter * 1.15f, IndicatorDiameter * 0.5f, TEXT("CONNECTED"));
    const float EndpointLength = FMath::Max(0.01f, Frame.CellSize - static_cast<float>(FittedIndicatorCenter.Y) * 2.0f);
    const bool bVertical = FMath::Abs(FVector::DotProduct(Up, FVector(0, 1, 0))) > 0.5;
    PlaceLabel(EndpointLabel, LabelMount, bVertical ? IndicatorDiameter * 0.7f : EndpointLength * 0.9f,
        bVertical ? EndpointLength * 0.9f : IndicatorDiameter * 0.7f,
        FittedPiece == EMazePipePiece::Inlet ? TEXT("START") : TEXT("EXIT"));
    FlowIndicatorMaterial = FlowIndicator->CreateDynamicMaterialInstance(0, IndicatorMaterial.Get());
    RequiredIndicatorMaterial = bRequiredChamber ? RequiredIndicator->CreateDynamicMaterialInstance(0, IndicatorMaterial.Get()) : nullptr;
    CachePipeSurfaceMaterials();
    SetIndicatorStates(EMazePipeFlowLightState::Idle, EMazePipeChamberLightState::Required);
}

void AMazePipePiece::CachePipeSurfaceMaterials()
{
    RestorePipeSurfaceMaterials();
    PipeSurfaceMaterials.Reset();
    TArray<UStaticMeshComponent*> Meshes = { PipeMesh };
    for (UStaticMeshComponent* Sleeve : SleeveMeshes)
    {
        Meshes.Add(Sleeve);
    }
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        if (!IsValid(Mesh))
        {
            continue;
        }
        FMazePipeSurfaceMaterials State;
        State.Mesh = Mesh;
        State.OverlayMaterial = Mesh->GetOverlayMaterial();
        State.bForceDisableNanite = Mesh->bForceDisableNanite;
        for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
        {
            State.Materials.Add(Mesh->GetMaterial(Slot));
        }
        PipeSurfaceMaterials.Add(State);
    }
    PipeSurfaceGlowMaterial = IsValid(IndicatorMaterial) ? UMaterialInstanceDynamic::Create(IndicatorMaterial.Get(), this) : nullptr;
    bSurfaceUsesOverlay = IsValid(IndicatorMaterial) && IndicatorMaterial->GetBlendMode() == BLEND_Additive;
}

void AMazePipePiece::RestorePipeSurfaceMaterials()
{
    for (const FMazePipeSurfaceMaterials& State : PipeSurfaceMaterials)
    {
        if (!IsValid(State.Mesh))
        {
            continue;
        }
        for (int32 Slot = 0; Slot < State.Materials.Num(); ++Slot)
        {
            State.Mesh->SetMaterial(Slot, State.Materials[Slot].Get());
        }
        State.Mesh->SetOverlayMaterial(State.OverlayMaterial.Get());
        State.Mesh->SetForceDisableNanite(State.bForceDisableNanite);
    }
}

void AMazePipePiece::ApplyPipeSurfaceGlow(const FLinearColor& Color, bool bLit)
{
    if (!bGlowWholePipe || !bLit || !IsValid(PipeSurfaceGlowMaterial))
    {
        RestorePipeSurfaceMaterials();
        return;
    }
    PipeSurfaceGlowMaterial->SetVectorParameterValue(TEXT("IndicatorColor"), Color);
    PipeSurfaceGlowMaterial->SetScalarParameterValue(TEXT("IndicatorGlow"), FMath::Max(0.0f, PipeFlowGlow));
    for (const FMazePipeSurfaceMaterials& State : PipeSurfaceMaterials)
    {
        if (!IsValid(State.Mesh))
        {
            continue;
        }
        if (bSurfaceUsesOverlay)
        {
            for (int32 Slot = 0; Slot < State.Materials.Num(); ++Slot)
            {
                State.Mesh->SetMaterial(Slot, State.Materials[Slot].Get());
            }
            // Translucent overlays use the component's regular mesh rendering path.
            State.Mesh->SetForceDisableNanite(true);
            State.Mesh->SetOverlayMaterial(PipeSurfaceGlowMaterial.Get());
        }
        else
        {
            State.Mesh->SetOverlayMaterial(State.OverlayMaterial.Get());
            State.Mesh->SetForceDisableNanite(State.bForceDisableNanite);
            for (int32 Slot = 0; Slot < State.Materials.Num(); ++Slot)
            {
                State.Mesh->SetMaterial(Slot, PipeSurfaceGlowMaterial.Get());
            }
        }
    }
}

void AMazePipePiece::ApplyIndicator(UStaticMeshComponent* Mesh, UPointLightComponent* Light,
    UMaterialInstanceDynamic* Material, const FLinearColor& Color, bool bLit)
{
    Mesh->SetVisibility(true);
    if (IsValid(Material))
    {
        Material->SetVectorParameterValue(TEXT("IndicatorColor"), Color);
        Material->SetScalarParameterValue(TEXT("IndicatorGlow"), bLit ? FMath::Max(0.0f, IndicatorGlow) : 0.1f);
    }
    Light->SetLightColor(Color);
    Light->SetVisibility(bLit && (bUsePointLights || !IsValid(IndicatorMaterial)));
}

void AMazePipePiece::SetIndicatorStates(EMazePipeFlowLightState FlowState, EMazePipeChamberLightState ChamberState)
{
    FlowLightState = FlowState;
    ChamberLightState = ChamberState;
    const bool bEndpoint = FittedPiece == EMazePipePiece::Inlet || FittedPiece == EMazePipePiece::Outlet;
    const FLinearColor EndpointColor = FittedPiece == EMazePipePiece::Inlet ? InletLightColor : OutletLightColor;
    const FLinearColor FlowColor = FlowState == EMazePipeFlowLightState::Completed ? ConnectedLightColor
        : FlowState == EMazePipeFlowLightState::Leak ? LeakLightColor
        : FlowState == EMazePipeFlowLightState::Flowing ? SteamLightColor
        : bEndpoint ? EndpointColor : FLinearColor(0.03f, 0.03f, 0.03f);
    const FLinearColor ChamberColor = ChamberState == EMazePipeChamberLightState::Connected ? ConnectedLightColor
        : ChamberState == EMazePipeChamberLightState::Bypassed ? LeakLightColor : RequiredLightColor;
    if (bGlowWholePipe)
    {
        const bool bLit = FlowState != EMazePipeFlowLightState::Idle || bRequiredChamber || bEndpoint;
        const FLinearColor Color = FlowState == EMazePipeFlowLightState::Completed ? ConnectedLightColor
            : FlowState == EMazePipeFlowLightState::Leak ? LeakLightColor
            : bRequiredChamber ? ChamberColor : FlowColor;
        FlowIndicator->SetVisibility(false);
        RequiredIndicator->SetVisibility(false);
        RequiredLight->SetVisibility(false);
        FlowLight->SetLightColor(Color);
        FlowLight->SetVisibility(bLit && bUsePointLights);
        ApplyPipeSurfaceGlow(Color, bLit);
    }
    else
    {
        RestorePipeSurfaceMaterials();
        ApplyIndicator(FlowIndicator, FlowLight, FlowIndicatorMaterial, FlowColor, FlowState != EMazePipeFlowLightState::Idle || bEndpoint);
        if (bRequiredChamber)
        {
            ApplyIndicator(RequiredIndicator, RequiredLight, RequiredIndicatorMaterial, ChamberColor, true);
        }
        else
        {
            RequiredIndicator->SetVisibility(false);
            RequiredLight->SetVisibility(false);
        }
    }
    RequiredLabel->SetVisibility(bRequiredChamber && bShowChamberLabel);
    RequiredLabel->SetTextRenderColor(FColor::Black);
    RequiredLabel->SetText(FText::FromString(ChamberState == EMazePipeChamberLightState::Connected ? TEXT("CONNECTED")
        : ChamberState == EMazePipeChamberLightState::Bypassed ? TEXT("BYPASS") : TEXT("REQUIRED")));
    EndpointLabel->SetVisibility(bEndpoint && bShowEndpointLabels);
    EndpointLabel->SetText(FText::FromString(FittedPiece == EMazePipePiece::Inlet ? TEXT("START") : TEXT("EXIT")));
    EndpointLabel->SetTextRenderColor(FColor::Black);
}