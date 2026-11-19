#include "Enviornment/MazePipeTrap.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enviornment/MazePipePuzzle.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"

USceneComponent* FMazePipeTrap::FindRoot(AActor* Room, FName Tag, FString& Error)
{
    if (!IsValid(Room) || Tag.IsNone())
    {
        Error = TEXT("Provide a room and a nonempty wall root Component Tag.");
        return nullptr;
    }
    USceneComponent* Found = nullptr;
    TArray<USceneComponent*> Components;
    Room->GetComponents<USceneComponent>(Components);
    for (USceneComponent* Component : Components)
    {
        if (IsValid(Component) && Component->ComponentHasTag(Tag))
        {
            if (Found)
            {
                Error = FString::Printf(TEXT("More than one room component has wall root tag %s."), *Tag.ToString());
                return nullptr;
            }
            Found = Component;
        }
    }
    if (!Found || Found == Room->GetRootComponent())
    {
        Error = FString::Printf(TEXT("Provide one separate wall root with Component Tag %s. Its name alone is not a tag; do not tag the room root."), *Tag.ToString());
        return nullptr;
    }
    return Found;
}

FTransform FMazePipeTrap::MakePose(const FTransform& Original, const FVector& Pivot,
    const FVector& Up, const FVector& Inward, float Radians, float Travel)
{
    const FQuat Turn(Up, Radians);
    return FTransform(Turn * Original.GetRotation(),
        Pivot + Turn.RotateVector(Original.GetLocation() - Pivot) + Inward * Travel,
        Original.GetScale3D());
}

bool FMazePipeTrap::IsSpike(const USceneComponent* Component, const USceneComponent* Root, FName Tag)
{
    for (const USceneComponent* Current = Component; IsValid(Current) && Current != Root; Current = Current->GetAttachParent())
    {
        if (Current->ComponentHasTag(Tag))
        {
            return true;
        }
    }
    return false;
}

bool FMazePipeTrap::ReadWall(AActor* Room, FName Tag, FName SpikeTag, FWall& Wall, FString& Error)
{
    USceneComponent* Root = FindRoot(Room, Tag, Error);
    if (!Root)
    {
        return false;
    }
    Wall.Root = Root;
    if (Root->ComponentHasTag(SpikeTag))
    {
        Error = TEXT("Put PipeTrapSpike on the spike meshes/group, not on the whole wall root.");
        return false;
    }
    Wall.OriginalWorld = Root->GetComponentTransform();
    Wall.OriginalRelative = Root->GetRelativeTransform();
    const FVector Scale = Wall.OriginalWorld.GetScale3D();
    if (Wall.OriginalWorld.ContainsNaN() || Scale.X <= 0.0 || Scale.Y <= 0.0 || Scale.Z <= 0.0)
    {
        Error = TEXT("Wall roots need finite transforms and positive, nonzero scale.");
        return false;
    }
    TArray<USceneComponent*> Children;
    Root->GetChildrenComponents(true, Children);
    Children.Insert(Root, 0);
    bool bHasSpikes = false;
    for (USceneComponent* Child : Children)
    {
        if (!IsValid(Child))
        {
            continue;
        }
        if (Child != Root && (Child->IsUsingAbsoluteLocation() || Child->IsUsingAbsoluteRotation() || Child->IsUsingAbsoluteScale()))
        {
            Error = FString::Printf(TEXT("%s uses an absolute transform. Wall children must inherit their parent's transform."), *Child->GetName());
            return false;
        }
        FSceneState State;
        State.Component = Child;
        State.Mobility = Child->Mobility;
        Scenes.Add(State);
        UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
        if (!IsValid(Primitive))
        {
            continue;
        }
        if (Primitive->IsSimulatingPhysics())
        {
            Error = FString::Printf(TEXT("Disable Simulate Physics on wall component %s."), *Child->GetName());
            return false;
        }
        if (!IsSpike(Child, Root, SpikeTag))
        {
            continue;
        }
        const FTransform LocalToRoot = Child->GetComponentTransform().GetRelativeTransform(Wall.OriginalWorld);
        const FBoxSphereBounds Bounds = Primitive->CalcBounds(LocalToRoot);
        if (Bounds.Origin.ContainsNaN() || Bounds.BoxExtent.ContainsNaN()
            || Bounds.BoxExtent.X <= 0.0 || Bounds.BoxExtent.Y <= 0.0 || Bounds.BoxExtent.Z <= 0.0)
        {
            Error = FString::Printf(TEXT("Tagged spike component %s has invalid or empty bounds."), *Child->GetName());
            return false;
        }
        bHasSpikes = true;
        FSpikeCollisionState Collision;
        Collision.Component = Primitive;
        Collision.Collision = Primitive->GetCollisionEnabled();
        SpikeCollisions.Add(Collision);
    }
    if (!bHasSpikes)
    {
        Error = FString::Printf(TEXT("Wall %s needs spike meshes beneath it with Component Tag %s (or a tagged spike-only group/ChildActor component)."), *Tag.ToString(), *SpikeTag.ToString());
        return false;
    }
    return true;
}

FTransform FMazePipeTrap::GetGridSpace(const FMazePipeGridFrame& Frame, float PipeWallOffset)
{
    const FVector Pivot = Frame.BottomLeft + Frame.Right * (Frame.Columns * Frame.CellSize * 0.5)
        + Frame.Up * (Frame.Rows * Frame.CellSize * 0.5) - Frame.Inward * PipeWallOffset;
    return FTransform(FRotationMatrix::MakeFromXZ(Frame.Inward, Frame.Up).ToQuat(), Pivot);
}

bool FMazePipeTrap::ReadPanelBounds(AActor* Room, USceneComponent* Panel, const FTransform& Space,
    FName SpikeTag, FBox& Bounds, FString& Error)
{
    Bounds = FBox(ForceInit);
    TArray<USceneComponent*> Parts;
    Panel->GetChildrenComponents(true, Parts);
    Parts.Insert(Panel, 0);
    for (USceneComponent* Part : Parts)
    {
        UPrimitiveComponent* Mesh = Cast<UPrimitiveComponent>(Part);
        if (!IsValid(Mesh) || IsSpike(Part, Panel, SpikeTag))
        {
            continue;
        }
        // Include native meshes and ChildActor meshes, but not arbitrary actors
        // (the generated pipe actors) attached to an inside panel.
        AActor* Actor = Mesh->GetOwner();
        while (IsValid(Actor) && Actor != Room) { Actor = Actor->GetParentActor(); }
        if (Actor != Room) { continue; }
        const FBoxSphereBounds B = Mesh->CalcBounds(Mesh->GetComponentTransform().GetRelativeTransform(Space));
        if (B.Origin.ContainsNaN() || B.BoxExtent.ContainsNaN())
        {
            Error = FString::Printf(TEXT("Panel %s has non-finite mesh bounds."), *Panel->GetName());
            return false;
        }
        Bounds += B.Origin - B.BoxExtent;
        Bounds += B.Origin + B.BoxExtent;
    }
    if (!Bounds.IsValid || Bounds.GetSize().Y <= 0.01 || Bounds.GetSize().Z <= 0.01)
    {
        Error = FString::Printf(TEXT("Panel %s needs a mesh with usable width and height."), *Panel->GetName());
        return false;
    }
    return true;
}

bool FMazePipeTrap::ReadPanels(AActor* Room, USceneComponent* Root, const FMazePipeGridFrame& Frame,
    float PipeWallOffset, const FMazePipeTrapSettings& Settings, TArray<FPanel>& Panels, FString& Error)
{
    Panels.Reset();
    const FTransform Space = GetGridSpace(Frame, PipeWallOffset);
    TArray<USceneComponent*> Children, Inside, Outside;
    Root->GetChildrenComponents(true, Children);
    for (USceneComponent* Child : Children)
    {
        if (!IsValid(Child) || Child->GetOwner() != Room) { continue; }
        const bool bOutside = Child->ComponentHasTag(Settings.OutsideWallTag);
        const bool bInside = (!Settings.InsideWallTag.IsNone() && Child->ComponentHasTag(Settings.InsideWallTag))
            || Child->GetName().StartsWith(TEXT("Inside"), ESearchCase::IgnoreCase);
        if (!bOutside && !bInside) { continue; }
        if (bOutside && bInside)
        {
            Error = FString::Printf(TEXT("Panel %s is selected as both inside and outside."), *Child->GetName());
            return false;
        }
        bool bNested = false;
        for (USceneComponent* P = Child->GetAttachParent(); IsValid(P) && P != Root; P = P->GetAttachParent())
        {
            if (P->ComponentHasTag(Settings.OutsideWallTag)
                || (!Settings.InsideWallTag.IsNone() && P->ComponentHasTag(Settings.InsideWallTag))
                || P->GetName().StartsWith(TEXT("Inside"), ESearchCase::IgnoreCase))
            {
                bNested = true;
                break;
            }
        }
        if (!bNested) { (bOutside ? Outside : Inside).Add(Child); }
    }
    if (Inside.IsEmpty() || Inside.Num() != Outside.Num())
    {
        Error = FString::Printf(TEXT("%s needs equal inside/outside panel counts; found %d inside and %d outside. InsideL/R names are recognized, or tag each inside panel %s."),
            *Root->GetName(), Inside.Num(), Outside.Num(), *Settings.InsideWallTag.ToString());
        return false;
    }
    TArray<FBox> InsideBounds;
    for (USceneComponent* C : Inside)
    {
        FBox B(ForceInit);
        if (!ReadPanelBounds(Room, C, Space, Settings.SpikeTag, B, Error)) { return false; }
        InsideBounds.Add(B);
    }
    TArray<USceneComponent*> Used;
    for (USceneComponent* Outer : Outside)
    {
        FBox OuterBounds(ForceInit);
        if (!ReadPanelBounds(Room, Outer, Space, Settings.SpikeTag, OuterBounds, Error)) { return false; }
        FString Expected;
        if (Outer->GetName().StartsWith(TEXT("Outside"), ESearchCase::IgnoreCase))
        {
            Expected = FString::Printf(TEXT("Inside%s"), *Outer->GetName().Mid(7));
        }
        int32 Match = INDEX_NONE;
        for (int32 I = 0; I < Inside.Num(); ++I)
        {
            if (!Used.Contains(Inside[I]) && !Expected.IsEmpty() && Inside[I]->GetName().Equals(Expected, ESearchCase::IgnoreCase))
            {
                Match = I;
                break;
            }
        }
        if (Match == INDEX_NONE)
        {
            double Best = 0.0;
            bool bAmbiguous = false;
            for (int32 I = 0; I < Inside.Num(); ++I)
            {
                if (Used.Contains(Inside[I])) { continue; }
                const FVector D = InsideBounds[I].GetCenter() - OuterBounds.GetCenter();
                const double Distance = D.Y * D.Y + D.Z * D.Z;
                if (Match == INDEX_NONE || Distance < Best - 0.01)
                {
                    Match = I; Best = Distance; bAmbiguous = false;
                }
                else if (FMath::Abs(Distance - Best) <= 0.01) { bAmbiguous = true; }
            }
            if (bAmbiguous)
            {
                Error = FString::Printf(TEXT("Cannot uniquely pair %s. Name its counterpart InsideL/R with the same number as OutsideL/R."), *Outer->GetName());
                return false;
            }
        }
        if (Match == INDEX_NONE) { Error = TEXT("A wall panel has no unused inside counterpart."); return false; }
        USceneComponent* Inner = Inside[Match];
        const FVector A = InsideBounds[Match].GetCenter(), B = OuterBounds.GetCenter();
        const double Width = FMath::Max(InsideBounds[Match].GetSize().Y, OuterBounds.GetSize().Y);
        const double Height = FMath::Max(InsideBounds[Match].GetSize().Z, OuterBounds.GetSize().Z);
        if (B.X >= A.X - 0.01 || FMath::Abs(A.Y - B.Y) > Width * 0.25 || FMath::Abs(A.Z - B.Z) > Height * 0.25)
        {
            Error = FString::Printf(TEXT("Panel pair %s / %s must line up across the wall thickness, with the outside panel behind the inside panel."),
                *Inner->GetName(), *Outer->GetName());
            return false;
        }
        if (Inner->IsAttachedTo(Outer) || Outer->IsAttachedTo(Inner))
        {
            Error = TEXT("Inside and outside panels must be separate siblings, not parents of one another.");
            return false;
        }
        FPanel Pair;
        Pair.Inside = Inner; Pair.Outside = Outer;
        Pair.InsideWorld = Inner->GetComponentTransform(); Pair.OutsideWorld = Outer->GetComponentTransform();
        Pair.InsideRelative = Inner->GetRelativeTransform(); Pair.OutsideRelative = Outer->GetRelativeTransform();
        if (Pair.InsideWorld.ContainsNaN() || Pair.OutsideWorld.ContainsNaN())
        {
            Error = TEXT("A panel pair has an invalid transform."); return false;
        }
        Pair.InsideBounds = InsideBounds[Match]; Pair.OutsideBounds = OuterBounds;
        Pair.Pivot = Space.TransformPosition((A + B) * 0.5);
        const bool bHorizontal = Settings.PanelFlipAxis == EMazePipePanelFlipAxis::Right
            || (Settings.PanelFlipAxis == EMazePipePanelFlipAxis::Auto && Width >= Height);
        Pair.Axis = bHorizontal ? Frame.Right : Frame.Up;
        Panels.Add(Pair);
        Used.Add(Inner);
    }
    return true;
}

int32 FMazePipeTrap::FindPanelIndex(const TArray<FPanel>& Panels, const FVector& Point, bool bOutside)
{
    int32 BestIndex = INDEX_NONE;
    double BestGap = 0.0, BestCenter = 0.0;
    for (int32 I = 0; I < Panels.Num(); ++I)
    {
        const FBox& B = bOutside ? Panels[I].OutsideBounds : Panels[I].InsideBounds;
        const double DY = FMath::Max(0.0, FMath::Max(B.Min.Y - Point.Y, Point.Y - B.Max.Y));
        const double DZ = FMath::Max(0.0, FMath::Max(B.Min.Z - Point.Z, Point.Z - B.Max.Z));
        const double Gap = DY * DY + DZ * DZ;
        const FVector D = B.GetCenter() - Point;
        const double Center = D.Y * D.Y + D.Z * D.Z;
        if (BestIndex == INDEX_NONE || Gap < BestGap - 0.0001
            || (FMath::Abs(Gap - BestGap) <= 0.0001 && Center < BestCenter))
        {
            BestIndex = I; BestGap = Gap; BestCenter = Center;
        }
    }
    return BestIndex;
}

USceneComponent* FMazePipeTrap::FindPipePanelParent(AActor* Room, const FMazePipeTrapSettings& Settings,
    bool bRightWall, const FMazePipeGridFrame& Frame, float PipeWallOffset,
    const FVector& Cell, FString& Error) const
{
    USceneComponent* Root = FindRoot(Room, bRightWall ? Settings.RightWallRootTag : Settings.LeftWallRootTag, Error);
    if (!Root) { return nullptr; }
    TArray<FPanel> Panels;
    if (!ReadPanels(Room, Root, Frame, PipeWallOffset, Settings, Panels, Error)) { return nullptr; }
    const FVector P = GetGridSpace(Frame, PipeWallOffset).InverseTransformPosition(Cell);
    const int32 Index = FindPanelIndex(Panels, P, false);
    return Panels.IsValidIndex(Index) ? Panels[Index].Inside.Get() : nullptr;
}

bool FMazePipeTrap::ReadFlippedSpikeBounds(const FWall& Wall, int32 Side, FName SpikeTag, FBox& Bounds, FString& Error)
{
    Bounds = FBox(ForceInit);
    TArray<USceneComponent*> Children;
    Wall.Root->GetChildrenComponents(true, Children);
    for (USceneComponent* Child : Children)
    {
        UPrimitiveComponent* Mesh = Cast<UPrimitiveComponent>(Child);
        if (!IsValid(Mesh) || !IsSpike(Child, Wall.Root.Get(), SpikeTag)) { continue; }
        const FPanel* Pair = nullptr;
        for (const FPanel& P : Wall.Panels)
        {
            if (Child == P.Outside.Get() || Child->IsAttachedTo(P.Outside.Get())) { Pair = &P; break; }
        }
        if (!Pair)
        {
            Error = FString::Printf(TEXT("Spike component %s must be under its individual outside panel."), *Child->GetName());
            return false;
        }
        const FTransform Relative = Mesh->GetComponentTransform().GetRelativeTransform(Pair->OutsideWorld);
        const FTransform Flipped = MakePose(Pair->OutsideWorld, Pair->Pivot, Pair->Axis, Wall.Inward, Side == 0 ? PI : -PI, 0.0f);
        const FTransform Future = Relative * Flipped;
        const FBoxSphereBounds B = Mesh->CalcBounds(Future.GetRelativeTransform(Wall.OriginalWorld));
        if (B.Origin.ContainsNaN() || B.BoxExtent.ContainsNaN()) { Error = TEXT("Flipped spike bounds are invalid."); return false; }
        Bounds += B.Origin - B.BoxExtent; Bounds += B.Origin + B.BoxExtent;
    }
    if (!Bounds.IsValid) { Error = TEXT("No spike bounds were found on the outside panels."); return false; }
    return true;
}

bool FMazePipeTrap::BuildSpikes(AActor* Room, const FMazePipeTrapSettings& Settings,
    const FMazePipeGridFrame& LeftFrame, const FMazePipeGridFrame& RightFrame,
    USceneComponent* LeftPipeParent, USceneComponent* RightPipeParent,
    float PipeWallOffset, bool bPreview, FString& Error)
{
    Error.Reset();
    if (bStarted)
    {
        Error = TEXT("Cannot regenerate spikes while the wall trap is running.");
        return false;
    }
    if (!Settings.bEnabled || !Settings.bGenerateSpikesFromGrid)
    {
        ClearSpikes();
        return true;
    }
    if (!IsValid(Room) || !Room->GetWorld() || Room->GetWorld()->IsGameWorld() == bPreview
        || !IsValid(Settings.SpikeMesh) || Settings.OutsideWallTag.IsNone() || Settings.SpikeTag.IsNone()
        || Settings.OutsideWallTag == Settings.SpikeTag
        || Settings.LeftWallRootTag == Settings.RightWallRootTag
        || Settings.OutsideWallTag == Settings.LeftWallRootTag || Settings.OutsideWallTag == Settings.RightWallRootTag
        || Settings.SpikeTag == Settings.LeftWallRootTag || Settings.SpikeTag == Settings.RightWallRootTag
        || !FMath::IsFinite(Settings.SpikeLength) || Settings.SpikeLength < 1.0f || Settings.SpikeLength > 500.0f
        || !FMath::IsFinite(Settings.SpikeDiameterFraction) || Settings.SpikeDiameterFraction < 0.05f || Settings.SpikeDiameterFraction > 0.9f
        || !FMath::IsFinite(Settings.SpikeSurfaceOffset) || Settings.SpikeSurfaceOffset < 0.0f || Settings.SpikeSurfaceOffset > 20.0f
        || !FMath::IsFinite(PipeWallOffset) || PipeWallOffset < 0.0f)
    {
        Error = TEXT("Spike grid needs a cone mesh, distinct nonempty tags, valid world/preview mode and valid spike size settings.");
        return false;
    }
    const FBox NativeBounds = Settings.SpikeMesh->GetBoundingBox();
    const FVector NativeSize = NativeBounds.GetSize();
    if (!NativeBounds.IsValid || NativeSize.ContainsNaN() || NativeBounds.Min.ContainsNaN() || NativeBounds.Max.ContainsNaN()
        || NativeSize.X <= 0.0001 || NativeSize.Y <= 0.0001 || NativeSize.Z <= 0.0001)
    {
        Error = TEXT("Spike Mesh needs finite, nonempty bounds, with its cone tip along local +Z.");
        return false;
    }
    USceneComponent* Roots[2] = { FindRoot(Room, Settings.LeftWallRootTag, Error), nullptr };
    if (!Roots[0]) { return false; }
    Roots[1] = FindRoot(Room, Settings.RightWallRootTag, Error);
    if (!Roots[1]) { return false; }
    if (Roots[0] == Roots[1] || Roots[0]->IsAttachedTo(Roots[1]) || Roots[1]->IsAttachedTo(Roots[0]))
    {
        Error = TEXT("Left/right wall roots must be separate assemblies.");
        return false;
    }
    USceneComponent* PipeParents[2] = { LeftPipeParent, RightPipeParent };
    const FMazePipeGridFrame Frames[2] = { LeftFrame, RightFrame };
    TArray<FPanel> RevealedPanels[2];
    TArray<TArray<FTransform>> Instances[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        USceneComponent* Root = Roots[Side];
        const FTransform RootTransform = Root->GetComponentTransform();
        const FVector RootScale = RootTransform.GetScale3D();
        const FMazePipeGridFrame& Frame = Frames[Side];
        const TCHAR* SideName = Side == 0 ? TEXT("Left") : TEXT("Right");
        if (!IsValid(PipeParents[Side]))
        {
            Error = FString::Printf(TEXT("%s wall %s: no unique pipe attachment root was resolved. Check MazePuzzle's wall root tags and keep exactly one matching component per tag."), SideName, *Root->GetName());
            return false;
        }
        if (PipeParents[Side] != Root && !PipeParents[Side]->IsAttachedTo(Root))
        {
            Error = FString::Printf(TEXT("%s wall: pipe attachment parent %s is outside tagged root %s. Use the matching MazePuzzleComponent.cpp; grid arrows can stay under DefaultSceneRoot."),
                SideName, *PipeParents[Side]->GetName(), *Root->GetName());
            return false;
        }
        if (Root->ComponentHasTag(Settings.OutsideWallTag))
        {
            Error = FString::Printf(TEXT("%s wall root %s incorrectly has %s. Keep that tag only on OutsideL/R children."),
                SideName, *Root->GetName(), *Settings.OutsideWallTag.ToString());
            return false;
        }
        if (RootTransform.ContainsNaN())
        {
            Error = FString::Printf(TEXT("%s wall root %s has a non-finite world transform."), SideName, *Root->GetName());
            return false;
        }
        if (RootScale.X <= 0.0 || RootScale.Y <= 0.0 || RootScale.Z <= 0.0)
        {
            Error = FString::Printf(TEXT("%s wall root %s has invalid WORLD scale (%g, %g, %g). Check the spawned room actor and its parent scale too. Outside/inside child mesh scales do not need to be 1."),
                SideName, *Root->GetName(), static_cast<double>(RootScale.X), static_cast<double>(RootScale.Y), static_cast<double>(RootScale.Z));
            return false;
        }
        if (Frame.Columns < 1 || Frame.Columns > 32 || Frame.Rows < 1 || Frame.Rows > 32 || Frame.Columns * Frame.Rows > 512)
        {
            Error = FString::Printf(TEXT("%s pipe grid has invalid dimensions: %d columns x %d rows. Maximum is 32 per axis and 512 cells."), SideName, Frame.Columns, Frame.Rows);
            return false;
        }
        if (!FMath::IsFinite(Frame.CellSize) || Frame.CellSize <= 0.0f)
        {
            Error = FString::Printf(TEXT("%s pipe grid has invalid cell size %g cm."), SideName, static_cast<double>(Frame.CellSize));
            return false;
        }
        if (Frame.BottomLeft.ContainsNaN() || Frame.Right.ContainsNaN() || Frame.Up.ContainsNaN() || Frame.Inward.ContainsNaN())
        {
            Error = FString::Printf(TEXT("%s pipe grid contains a non-finite corner position or frame axis."), SideName);
            return false;
        }
        if (FMath::Abs(Frame.Right.Size() - 1.0) > 0.001 || FMath::Abs(Frame.Up.Size() - 1.0) > 0.001
            || FMath::Abs(Frame.Inward.Size() - 1.0) > 0.001)
        {
            Error = FString::Printf(TEXT("%s pipe grid has non-unit axes: Right=%g Up=%g Inward=%g."),
                SideName, Frame.Right.Size(), Frame.Up.Size(), Frame.Inward.Size());
            return false;
        }
        if (FMath::Abs(FVector::DotProduct(Frame.Right, Frame.Up)) > 0.001
            || FMath::Abs(FVector::DotProduct(Frame.Right, Frame.Inward)) > 0.001
            || FMath::Abs(FVector::DotProduct(Frame.Up, Frame.Inward)) > 0.001)
        {
            Error = FString::Printf(TEXT("%s pipe grid axes are not perpendicular: Right.Up=%g Right.Inward=%g Up.Inward=%g."),
                SideName, FVector::DotProduct(Frame.Right, Frame.Up), FVector::DotProduct(Frame.Right, Frame.Inward), FVector::DotProduct(Frame.Up, Frame.Inward));
            return false;
        }
        TArray<FPanel>& Panels = RevealedPanels[Side];
        if (!ReadPanels(Room, Root, Frame, PipeWallOffset, Settings, Panels, Error))
        {
            return false;
        }
        Instances[Side].SetNum(Panels.Num());
        const FTransform Space = GetGridSpace(Frame, PipeWallOffset);
        const float FlipAngle = Side == 0 ? PI : -PI;
        // Lay out a single grid on the revealed wall. Reflecting the original grid
        // around each panel's own pivot can change the spacing at the seams.
        for (FPanel& Panel : Panels)
        {
            const FTransform GridFlip = MakePose(FTransform::Identity,
                Space.InverseTransformPosition(Panel.Pivot), Space.GetRotation().UnrotateVector(Panel.Axis),
                FVector::ZeroVector, FlipAngle, 0.0f);
            Panel.OutsideBounds = Panel.OutsideBounds.TransformBy(GridFlip);
            Panel.OutsideWorld = MakePose(Panel.OutsideWorld, Panel.Pivot, Panel.Axis, Frame.Inward, FlipAngle, 0.0f);
        }
        const FQuat OutwardRotation = FRotationMatrix::MakeFromXZ(Frame.Right, -Frame.Inward).ToQuat();
        const double Diameter = Frame.CellSize * Settings.SpikeDiameterFraction;
        const FVector Scale(Diameter / NativeSize.X, Diameter / NativeSize.Y, Settings.SpikeLength / NativeSize.Z);
        const FVector NativeBase(NativeBounds.GetCenter().X, NativeBounds.GetCenter().Y, NativeBounds.Min.Z);
        for (int32 Index = 0; Index < Frame.Columns * Frame.Rows; ++Index)
        {
            const FVector Cell = Frame.GetCellCenter(Index);
            const FVector Point = Space.InverseTransformPosition(Cell);
            const int32 PanelIndex = FindPanelIndex(Panels, Point, true);
            if (!Panels.IsValidIndex(PanelIndex)) { Error = TEXT("A spike cell has no outside panel."); return false; }
            const FPanel& Panel = Panels[PanelIndex];
            const FQuat Rotation = FQuat(Panel.Axis, FlipAngle) * OutwardRotation;
            const double FaceDepth = Panel.OutsideBounds.Max.X;
            const FVector Base = Cell + Frame.Inward * (FaceDepth + Settings.SpikeSurfaceOffset - Point.X);
            const FTransform RevealedPose(Rotation, Base - Rotation.RotateVector(NativeBase * Scale), Scale);
            // This offset is local to the panel at every angle, including its starting pose.
            const FTransform LocalPose = RevealedPose.GetRelativeTransform(Panel.OutsideWorld);
            if (LocalPose.ContainsNaN())
            {
                Error = TEXT("A generated spike has an invalid transform.");
                return false;
            }
            Instances[Side][PanelIndex].Add(LocalPose);
        }
    }
    // Validation is complete before replacing either side's existing generated field.
    ClearSpikes();
    for (int32 Side = 0; Side < 2; ++Side)
    {
        for (int32 PanelIndex = 0; PanelIndex < RevealedPanels[Side].Num(); ++PanelIndex)
        {
            if (Instances[Side][PanelIndex].IsEmpty()) { continue; }
            UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(Room, NAME_None, RF_Transient | RF_DuplicateTransient);
            GeneratedSpikes.Add(Mesh);
            Room->AddInstanceComponent(Mesh);
            Mesh->SetupAttachment(RevealedPanels[Side][PanelIndex].Outside.Get());
            Mesh->SetRelativeTransform(FTransform::Identity);
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(Settings.SpikeMesh);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetGenerateOverlapEvents(false);
            Mesh->SetCanEverAffectNavigation(false);
            Mesh->ComponentTags.AddUnique(Settings.SpikeTag);
            Mesh->bIsEditorOnly = bPreview;
            if (IsValid(Settings.SpikeMaterial))
            {
                for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot) { Mesh->SetMaterial(Slot, Settings.SpikeMaterial); }
            }
            Mesh->RegisterComponent();
            for (const FTransform& Instance : Instances[Side][PanelIndex])
            {
                if (Mesh->AddInstance(Instance, false) == INDEX_NONE)
                {
                    Error = TEXT("Could not add a cone instance to the spike grid.");
                    ClearSpikes();
                    return false;
                }
            }
        }
    }
    return true;
}

void FMazePipeTrap::ClearSpikes()
{
    for (TWeakObjectPtr<UInstancedStaticMeshComponent>& Weak : GeneratedSpikes)
    {
        if (UInstancedStaticMeshComponent* Mesh = Weak.Get())
        {
            if (IsValid(Mesh->GetOwner())) { Mesh->GetOwner()->RemoveInstanceComponent(Mesh); }
            Mesh->DestroyComponent();
        }
        Weak.Reset();
    }
    GeneratedSpikes.Reset();
}

bool FMazePipeTrap::Start(AActor* Room, const FMazePipeTrapSettings& Settings,
    const FMazePipeGridFrame& LeftFrame, const FMazePipeGridFrame& RightFrame,
    USceneComponent* LeftPipeParent, USceneComponent* RightPipeParent, float PipeWallOffset, FString& Error,
    const UBoxComponent* RoomArea)
{
    Error.Reset();
    if (bStarted)
    {
        Error = TEXT("This room's pipe trap has already started.");
        return false;
    }
    ClearMotion();
    if (!Settings.bEnabled || !IsValid(Room) || !Room->GetWorld() || !Room->GetWorld()->IsGameWorld())
    {
        Error = TEXT("The wall trap can only start in a gameplay room with Enabled checked.");
        return false;
    }
    if (Settings.LeftWallRootTag.IsNone() || Settings.RightWallRootTag.IsNone() || Settings.SpikeTag.IsNone()
        || Settings.LeftWallRootTag == Settings.RightWallRootTag
        || Settings.SpikeTag == Settings.LeftWallRootTag || Settings.SpikeTag == Settings.RightWallRootTag
        || !FMath::IsFinite(Settings.TurnSeconds) || Settings.TurnSeconds < 0.1f || Settings.TurnSeconds > 10.0f
        || !FMath::IsFinite(Settings.CloseSeconds) || Settings.CloseSeconds < 0.1f || Settings.CloseSeconds > 120.0f
        || !FMath::IsFinite(Settings.FinalTipGap) || Settings.FinalTipGap < 0.0f || Settings.FinalTipGap > 100.0f
        || !FMath::IsFinite(PipeWallOffset) || PipeWallOffset < 0.0f)
    {
        Error = TEXT("Use distinct nonempty trap tags and the displayed positive timing/nonnegative gap ranges.");
        return false;
    }
    const FMazePipeGridFrame Frames[2] = { LeftFrame, RightFrame };
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FMazePipeGridFrame& Frame = Frames[Side];
        if (Frame.Columns <= 0 || Frame.Rows <= 0 || !FMath::IsFinite(Frame.CellSize) || Frame.CellSize <= 0.0f
            || Frame.BottomLeft.ContainsNaN() || Frame.Up.ContainsNaN() || Frame.Right.ContainsNaN() || Frame.Inward.ContainsNaN()
            || FMath::Abs(Frame.Up.Size() - 1.0) > 0.001 || FMath::Abs(Frame.Right.Size() - 1.0) > 0.001
            || FMath::Abs(Frame.Inward.Size() - 1.0) > 0.001
            || FMath::Abs(FVector::DotProduct(Frame.Up, Frame.Inward)) > 0.001)
        {
            Error = TEXT("The pipe wall frames contain invalid dimensions or axes.");
            return false;
        }
        Walls[Side].Pivot = Frame.BottomLeft + Frame.Right * (Frame.Columns * Frame.CellSize * 0.5)
            + Frame.Up * (Frame.Rows * Frame.CellSize * 0.5) - Frame.Inward * PipeWallOffset;
        Walls[Side].Up = Frame.Up;
        Walls[Side].Inward = Frame.Inward;
    }
    const FVector Delta = Walls[1].Pivot - Walls[0].Pivot;
    const double Separation = Delta.Size();
    if (!FMath::IsFinite(Separation) || Separation < 10.0
        || FVector::DotProduct(Walls[0].Inward, Walls[1].Inward) > -0.9999
        || FVector::DotProduct(Walls[0].Up, Walls[1].Up) < 0.9999
        || FVector::DotProduct(Delta.GetSafeNormal(), Walls[0].Inward) < 0.9999)
    {
        Error = TEXT("The two pipe-wall rectangles must face directly across the room, with matching up directions.");
        return false;
    }
    if (Settings.bGenerateSpikesFromGrid && !BuildSpikes(Room, Settings, LeftFrame, RightFrame,
        LeftPipeParent, RightPipeParent, PipeWallOffset, false, Error))
    {
        return false;
    }
    if (!ReadWall(Room, Settings.LeftWallRootTag, Settings.SpikeTag, Walls[0], Error)
        || !ReadWall(Room, Settings.RightWallRootTag, Settings.SpikeTag, Walls[1], Error))
    {
        return false;
    }
    if (Walls[0].Root.Get() == Walls[1].Root.Get()
        || Walls[0].Root->IsAttachedTo(Walls[1].Root.Get()) || Walls[1].Root->IsAttachedTo(Walls[0].Root.Get()))
    {
        Error = TEXT("Left and right wall roots must be separate assemblies, not children of one another.");
        return false;
    }
    USceneComponent* PipeParents[2] = { LeftPipeParent, RightPipeParent };
    for (int32 Side = 0; Side < 2; ++Side)
    {
        if (!IsValid(PipeParents[Side]) || (PipeParents[Side] != Walls[Side].Root.Get()
            && !PipeParents[Side]->IsAttachedTo(Walls[Side].Root.Get())))
        {
            Error = TEXT("Pipe attachment parents must resolve to their tagged wall roots. Use the matching MazePuzzleComponent.cpp; corner markers can remain under DefaultSceneRoot.");
            return false;
        }
    }
    const FVector Middle = (Walls[0].Pivot + Walls[1].Pivot) * 0.5;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        FWall& Wall = Walls[Side];
        FBox SpikeBounds(ForceInit);
        if (!ReadPanels(Room, Wall.Root.Get(), Frames[Side], PipeWallOffset, Settings, Wall.Panels, Error)
            || !ReadFlippedSpikeBounds(Wall, Side, Settings.SpikeTag, SpikeBounds, Error))
        {
            return false;
        }
        Wall.ContactSpace = GetGridSpace(Frames[Side], PipeWallOffset);
        Wall.ContactBounds = SpikeBounds.TransformBy(Wall.OriginalWorld.GetRelativeTransform(Wall.ContactSpace));
        const double LeadingTip = Wall.ContactBounds.Max.X;
        const double Travel = FVector::DotProduct(Middle - Wall.Pivot, Wall.Inward) - LeadingTip - Settings.FinalTipGap * 0.5;
        if (!FMath::IsFinite(LeadingTip) || LeadingTip <= 0.0 || !FMath::IsFinite(Travel) || Travel <= 0.0)
        {
            Error = TEXT("Place the spikes on the backs of both walls. After a 180 degree turn their tips must face inward and leave room to close.");
            return false;
        }
        Wall.Travel = static_cast<float>(Travel);

        // Keep the sweep depth tied to the spike tips. Cover the complete face,
        // including end recesses inside the authored room area.
        FBox Coverage(ForceInit);
        for (const FPanel& Panel : Wall.Panels)
        {
            Coverage += Panel.InsideBounds.Min;
            Coverage += Panel.InsideBounds.Max;
        }
        if (IsValid(RoomArea) && RoomArea->GetOwner() == Room)
        {
            const FVector Extent = RoomArea->GetUnscaledBoxExtent();
            const FTransform RoomToWall = RoomArea->GetComponentTransform().GetRelativeTransform(Wall.ContactSpace);
            if (!Extent.ContainsNaN() && Extent.X > 0.0 && Extent.Y > 0.0 && Extent.Z > 0.0 && !RoomToWall.ContainsNaN())
            {
                const FBox RoomBounds = FBox(-Extent, Extent).TransformBy(RoomToWall);
                Coverage += RoomBounds.Min;
                Coverage += RoomBounds.Max;
            }
        }
        Wall.ContactBounds += FVector(Wall.ContactBounds.Min.X, Coverage.Min.Y, Coverage.Min.Z);
        Wall.ContactBounds += FVector(Wall.ContactBounds.Max.X, Coverage.Max.Y, Coverage.Max.Z);
    }
    Owner = Room;
    ActiveSettings = Settings;
    // Make descendants movable before their parents, including attached runtime pipe
    // actors and meshes inside ChildActor components. Restore authored values on cleanup.
    for (int32 Index = Scenes.Num() - 1; Index >= 0; --Index)
    {
        if (Scenes[Index].Component.IsValid())
        {
            Scenes[Index].Component->SetMobility(EComponentMobility::Movable);
        }
    }
    for (const FSpikeCollisionState& State : SpikeCollisions)
    {
        if (State.Component.IsValid())
        {
            State.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    Elapsed = 0.0f;
    bHitReported = false;
    bStarted = true;
    Phase = EMazePipeTrapPhase::Flipping;
    return true;
}

void FMazePipeTrap::ApplyPose(float TurnAlpha, float CloseAlpha)
{
    const float TurnProgress = TurnAlpha * TurnAlpha * (3.0f - 2.0f * TurnAlpha);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        FWall& Wall = Walls[Side];
        if (Wall.Root.IsValid())
        {
            const float Travel = Wall.Travel * CloseAlpha;
            // Roots only translate. Each physical panel pair flips around its own center.
            Wall.Root->SetWorldTransform(FTransform(Wall.OriginalWorld.GetRotation(),
                Wall.OriginalWorld.GetLocation() + Wall.Inward * Travel, Wall.OriginalWorld.GetScale3D()),
                false, nullptr, ETeleportType::TeleportPhysics);
            const float Angle = (Side == 0 ? PI : -PI) * TurnProgress;
            for (const FPanel& Pair : Wall.Panels)
            {
                if (Pair.Inside.IsValid())
                {
                    Pair.Inside->SetWorldTransform(MakePose(Pair.InsideWorld, Pair.Pivot, Pair.Axis, Wall.Inward, Angle, Travel),
                        false, nullptr, ETeleportType::TeleportPhysics);
                }
                if (Pair.Outside.IsValid())
                {
                    Pair.Outside->SetWorldTransform(MakePose(Pair.OutsideWorld, Pair.Pivot, Pair.Axis, Wall.Inward, Angle, Travel),
                        false, nullptr, ETeleportType::TeleportPhysics);
                }
            }
        }
    }
}

bool FMazePipeTrap::HasPlayerContact(float PreviousCloseAlpha, float CloseAlpha) const
{
    if (!Owner.IsValid() || !Owner->GetWorld())
    {
        return false;
    }
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MazePipeTrap), false, Owner.Get());
    for (const FWall& Wall : Walls)
    {
        if (!Wall.Root.IsValid())
        {
            continue;
        }
        const float HalfSweep = Wall.Travel * (CloseAlpha - PreviousCloseAlpha) * 0.5f;
        const FVector Center = Wall.ContactSpace.TransformPosition(Wall.ContactBounds.GetCenter())
            + Wall.Inward * (Wall.Travel * CloseAlpha - HalfSweep);
        const FVector HalfExtent = Wall.ContactBounds.GetExtent() + FVector(HalfSweep, 0.0, 0.0);
        TArray<FOverlapResult> Contacts;
        Owner->GetWorld()->OverlapMultiByObjectType(Contacts, Center, Wall.ContactSpace.GetRotation(),
            Objects, FCollisionShape::MakeBox(HalfExtent), Query);
        for (const FOverlapResult& Contact : Contacts)
        {
            const APawn* Player = Cast<APawn>(Contact.GetActor());
            if (IsValid(Player) && Player->IsPlayerControlled() && Contact.Component.Get() == Player->GetRootComponent())
            {
                return true;
            }
        }
    }
    return false;
}

bool FMazePipeTrap::SwitchToSpikes()
{
    if (!IsActive()) { return false; }
    if (Phase == EMazePipeTrapPhase::Revealing || bWallsSwitched) { return true; }
    if (!Walls[0].Root.IsValid() || !Walls[1].Root.IsValid()) { return false; }
    // Start() captured the original panel transforms. Advance() samples the turn from those poses.
    Phase = EMazePipeTrapPhase::Revealing;
    return true;
}

bool FMazePipeTrap::Advance(float DeltaTime, bool bAllowContact)
{
    if (!IsActive() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
    {
        return false;
    }
    if (!Owner.IsValid() || !Owner->GetWorld() || !Owner->GetWorld()->IsGameWorld()
        || !Walls[0].Root.IsValid() || !Walls[1].Root.IsValid())
    {
        bHitReported = true;
        return false;
    }
    const float CloseStart = ActiveSettings.TurnSeconds;
    // Clamp the elapsed time to the end of the turn while Sequencer owns the player.
    const float EndTime = bAllowContact ? CloseStart + ActiveSettings.CloseSeconds : CloseStart;
    if (!bAllowContact && Elapsed >= EndTime) { return false; }
    const float Previous = FMath::Clamp((Elapsed - CloseStart) / ActiveSettings.CloseSeconds, 0.0f, 1.0f);
    Elapsed = FMath::Min(EndTime, Elapsed + DeltaTime);
    const float Turn = FMath::Clamp(Elapsed / CloseStart, 0.0f, 1.0f);
    const float Close = FMath::Clamp((Elapsed - CloseStart) / ActiveSettings.CloseSeconds, 0.0f, 1.0f);
    ApplyPose(Turn, Close);
    bWallsSwitched = Turn >= 1.0f;
    if (!bAllowContact)
    {
        Phase = EMazePipeTrapPhase::Revealing;
        return false;
    }
    Phase = Close >= 1.0f ? EMazePipeTrapPhase::Closed
        : Elapsed >= CloseStart ? EMazePipeTrapPhase::Closing : EMazePipeTrapPhase::Flipping;
    // Spike damage begins when the 180 degree turn completes. While closing, the
    // query includes the distance moved since the previous tick, including a hitch.
    if (bWallsSwitched && HasPlayerContact(Previous, Close))
    {
        bHitReported = true;
        return true;
    }
    return false;
}

void FMazePipeTrap::ClearMotion()
{
    if (bStarted)
    {
        for (FWall& Wall : Walls)
        {
            if (Wall.Root.IsValid())
            {
                Wall.Root->SetRelativeTransform(Wall.OriginalRelative, false, nullptr, ETeleportType::TeleportPhysics);
            }
            for (const FPanel& Pair : Wall.Panels)
            {
                if (Pair.Inside.IsValid()) { Pair.Inside->SetRelativeTransform(Pair.InsideRelative, false, nullptr, ETeleportType::TeleportPhysics); }
                if (Pair.Outside.IsValid()) { Pair.Outside->SetRelativeTransform(Pair.OutsideRelative, false, nullptr, ETeleportType::TeleportPhysics); }
            }
        }
        for (const FSpikeCollisionState& State : SpikeCollisions)
        {
            if (State.Component.IsValid())
            {
                State.Component->SetCollisionEnabled(State.Collision);
            }
        }
        for (const FSceneState& State : Scenes)
        {
            if (State.Component.IsValid())
            {
                State.Component->SetMobility(State.Mobility);
            }
        }
    }
    Owner.Reset();
    Walls[0] = FWall();
    Walls[1] = FWall();
    Scenes.Reset();
    SpikeCollisions.Reset();
    Elapsed = 0.0f;
    bStarted = false;
    bWallsSwitched = false;
    bHitReported = false;
    Phase = EMazePipeTrapPhase::Idle;
}

void FMazePipeTrap::Clear()
{
    ClearMotion();
    ClearSpikes();
}

bool FMazePipeTrapReveal::HasValidPlayer() const
{
    return Pawn.IsValid() && Controller.IsValid()
        && Controller->GetPawn() == Pawn.Get() && Pawn->GetController() == Controller.Get();
}

bool FMazePipeTrapReveal::LockPlayer(APawn* Player, FString& Error)
{
    Error.Reset();
    if (bLocked)
    {
        if (HasValidPlayer() && Pawn.Get() == Player) { return true; }
        Error = TEXT("The reveal lock belongs to a different or unavailable player.");
        return false;
    }
    APlayerController* PC = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!IsValid(PC) || !PC->IsLocalController()
        || PC->GetPawn() != Player || !Player->IsPlayerControlled())
    {
        Error = TEXT("The console sequence needs its locally controlled player.");
        return false;
    }
    Pawn = Player; Controller = PC;
    OriginalLook = PC->GetControlRotation();
    bPawnInputWasEnabled = Player->InputEnabled();
    // Balanced controller locks preserve any input suppression owned by other systems.
    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
    Player->DisableInput(PC);
    Player->ConsumeMovementInputVector();
    if (ACharacter* Character = Cast<ACharacter>(Player)) { Character->StopJumping(); }
    Movement = Player->GetMovementComponent();
    if (Movement.IsValid())
    {
        bMovementWasActive = Movement->IsActive();
        Movement->StopMovementImmediately();
        if (UCharacterMovementComponent* CharacterMove = Cast<UCharacterMovementComponent>(Movement.Get()))
        {
            OriginalMovementMode = static_cast<uint8>(CharacterMove->MovementMode);
            OriginalCustomMovementMode = CharacterMove->CustomMovementMode;
            CharacterMove->DisableMovement();
        }
        Movement->Deactivate();
    }
    bLocked = true;
    return true;
}

void FMazePipeTrapReveal::ApplyLook(float YawOffset, float Pitch)
{
    if (bLocked && HasValidPlayer() && FMath::IsFinite(YawOffset) && FMath::IsFinite(Pitch))
    {
        Controller->SetControlRotation(FRotator(FMath::Clamp(static_cast<double>(Pitch), -80.0, 80.0),
            OriginalLook.Yaw + FMath::Clamp(YawOffset, -360.0f, 360.0f), OriginalLook.Roll));
    }
}

void FMazePipeTrapReveal::UnlockPlayer()
{
    ReleasePlayer(false);
}

void FMazePipeTrapReveal::ReleasePlayer(bool bRestoreLook)
{
    if (bLocked)
    {
        const bool bSamePawn = Pawn.IsValid() && Controller.IsValid() && Controller->GetPawn() == Pawn.Get();
        if (Movement.IsValid())
        {
            Movement->StopMovementImmediately();
            if (UCharacterMovementComponent* CharacterMove = Cast<UCharacterMovementComponent>(Movement.Get()))
            {
                CharacterMove->SetMovementMode(static_cast<EMovementMode>(OriginalMovementMode), OriginalCustomMovementMode);
            }
            if (bMovementWasActive) { Movement->Activate(); }
        }
        if (Controller.IsValid())
        {
            Controller->SetIgnoreMoveInput(false);
            Controller->SetIgnoreLookInput(false);
            if (bRestoreLook && bSamePawn) { Controller->SetControlRotation(OriginalLook); }
        }
        if (bSamePawn && bPawnInputWasEnabled) { Pawn->EnableInput(Controller.Get()); }
    }
    bLocked = false;
}

void FMazePipeTrapReveal::Clear()
{
    ReleasePlayer(true);
    Pawn.Reset(); Controller.Reset(); Movement.Reset();
}