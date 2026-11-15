// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipeFitting.h"

namespace
{
    FVector PortDirection(int32 Direction)
    {
        const FVector Directions[] = { FVector(0, 1, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(-1, 0, 0) };
        return Directions[Direction];
    }
}

bool FMazePipeFitting::ReadSource(EMazePipePiece Piece, const TArray<FVector>& PortPositions, float Diameter,
    FMazePipeSourceGeometry& OutGeometry, FString& OutError)
{
    OutGeometry = FMazePipeSourceGeometry();
    OutError.Reset();
    if (PortPositions.Num() != 4 || !FMath::IsFinite(Diameter) || Diameter <= 0.01f)
    {
        OutError = TEXT("Provide four source connector positions and a positive native pipe diameter.");
        return false;
    }
    FMazePipeCell SourceCell;
    SourceCell.Piece = Piece;
    const uint8 Ports = FMazePipePuzzle::GetPorts(SourceCell);
    FVector Center;
    switch (Piece)
    {
    case EMazePipePiece::Straight:
    case EMazePipePiece::Chamber:
        Center = (PortPositions[0] + PortPositions[2]) * 0.5;
        break;
    case EMazePipePiece::Bend:
        Center = FVector(PortPositions[0].X, PortPositions[1].Y, (PortPositions[0].Z + PortPositions[1].Z) * 0.5);
        break;
    case EMazePipePiece::Tee:
        Center = FVector(PortPositions[0].X, (PortPositions[1].Y + PortPositions[3].Y) * 0.5,
            (PortPositions[0].Z + PortPositions[1].Z + PortPositions[3].Z) / 3.0);
        break;
    case EMazePipePiece::Cross:
        Center = FVector((PortPositions[0].X + PortPositions[2].X) * 0.5,
            (PortPositions[1].Y + PortPositions[3].Y) * 0.5,
            (PortPositions[0].Z + PortPositions[1].Z + PortPositions[2].Z + PortPositions[3].Z) * 0.25);
        break;
    default:
        OutError = TEXT("Use Straight, Bend, Tee, Cross or Chamber for the source pipe shape.");
        return false;
    }

    if (Center.ContainsNaN())
    {
        OutError = TEXT("A source connector has an invalid position.");
        return false;
    }
    OutGeometry.PortDistances.Init(0.0f, 4);
    const double Tolerance = FMath::Max(0.1, Diameter * 0.005);
    for (int32 Direction = 0; Direction < 4; ++Direction)
    {
        if (!(Ports & (1 << Direction)))
        {
            continue;
        }
        const FVector Delta = PortPositions[Direction] - Center;
        const double Reach = FVector::DotProduct(Delta, PortDirection(Direction));
        if (Delta.ContainsNaN() || Reach <= 0.01 || (Delta - PortDirection(Direction) * Reach).Size() > Tolerance)
        {
            OutError = TEXT("Source connectors must sit on the correct positive or negative X and Y axes through the pipe center. Check their positions in Top view.");
            return false;
        }
        OutGeometry.PortDistances[Direction] = static_cast<float>(Reach);
    }
    OutGeometry.Piece = Piece;
    OutGeometry.Center = Center;
    OutGeometry.Diameter = Diameter;
    OutGeometry.Ports = Ports;
    return true;
}

bool FMazePipeFitting::FitMesh(const FTransform& AuthoredTransform, const FMazePipeSourceGeometry& Geometry,
    float TargetDiameter, float TargetLength, FTransform& OutTransform, FString& OutError)
{
    OutTransform = FTransform::Identity;
    OutError.Reset();
    const FVector SourceScale = AuthoredTransform.GetScale3D();
    if (AuthoredTransform.ContainsNaN() || Geometry.Center.ContainsNaN() || Geometry.PortDistances.Num() != 4
        || !FMath::IsFinite(Geometry.Diameter) || Geometry.Diameter <= 0.01f
        || !FMath::IsFinite(TargetDiameter) || TargetDiameter <= 0.01f
        || !FMath::IsFinite(TargetLength) || TargetLength <= 0.01f
        || SourceScale.X <= 0.0 || SourceScale.Y <= 0.0 || SourceScale.Z <= 0.0)
    {
        OutError = TEXT("Pipe fitting needs finite transforms, positive scales and valid source and target measurements.");
        return false;
    }

    const double RadialScale = TargetDiameter / Geometry.Diameter;
    FVector NativeScale(RadialScale, RadialScale, RadialScale);
    FVector CanonicalScale = NativeScale;
    if (Geometry.Piece == EMazePipePiece::Straight)
    {
        const double SourceLength = Geometry.PortDistances[0] + Geometry.PortDistances[2];
        if (SourceLength <= 0.01)
        {
            OutError = TEXT("The straight pipe needs two different connector positions.");
            return false;
        }
        const FVector LengthAxis = AuthoredTransform.GetRotation().UnrotateVector(FVector(0, 1, 0));
        int32 Axis = 0;
        if (FMath::Abs(LengthAxis.Y) > FMath::Abs(LengthAxis[Axis]))
        {
            Axis = 1;
        }
        if (FMath::Abs(LengthAxis.Z) > FMath::Abs(LengthAxis[Axis]))
        {
            Axis = 2;
        }
        if (FMath::Abs(LengthAxis[Axis]) < 0.9999)
        {
            OutError = TEXT("Rotate the straight mesh in quarter turns so its length follows the Blueprint Y axis.");
            return false;
        }
        CanonicalScale.Y = TargetLength / SourceLength;
        NativeScale[Axis] = CanonicalScale.Y;
    }

    OutTransform = FTransform(AuthoredTransform.GetRotation(),
        (AuthoredTransform.GetTranslation() - Geometry.Center) * CanonicalScale, SourceScale * NativeScale);
    return true;
}

bool FMazePipeFitting::BuildSleeves(const FMazePipeSourceGeometry& Geometry, float CellSize, float TargetDiameter,
    bool bWallConnection, float WallReach, TArray<FMazePipeSleeve>& OutSleeves, FString& OutError)
{
    OutSleeves.Reset();
    OutError.Reset();
    if (!FMath::IsFinite(CellSize) || CellSize <= 0.01f || !FMath::IsFinite(TargetDiameter) || TargetDiameter <= 0.01f
        || !FMath::IsFinite(Geometry.Diameter) || Geometry.Diameter <= 0.01f || Geometry.PortDistances.Num() != 4
        || !FMath::IsFinite(WallReach) || WallReach < 0.0f || (bWallConnection && Geometry.Piece != EMazePipePiece::Bend))
    {
        OutError = TEXT("Check the pipe diameter, cell size and wall connection reach.");
        return false;
    }
    if (Geometry.Piece == EMazePipePiece::Straight)
    {
        return true;
    }
    const double Scale = TargetDiameter / Geometry.Diameter;
    for (int32 Direction = 0; Direction < 4; ++Direction)
    {
        if (!(Geometry.Ports & (1 << Direction)))
        {
            continue;
        }
        const double Reach = Geometry.PortDistances[Direction] * Scale;
        const double Target = bWallConnection && Direction == 1 ? FMath::Max(Reach, static_cast<double>(WallReach)) : CellSize * 0.5;
        if (Reach > Target + 0.01)
        {
            OutError = TEXT("A pipe body extends beyond its cell. Reduce Pipe Diameter Fraction or correct its source connectors.");
            OutSleeves.Reset();
            return false;
        }
        if (Target - Reach > 0.01)
        {
            FMazePipeSleeve Sleeve;
            Sleeve.Direction = Direction;
            Sleeve.Length = static_cast<float>(Target - Reach);
            Sleeve.Center = PortDirection(Direction) * ((Target + Reach) * 0.5);
            OutSleeves.Add(Sleeve);
        }
    }
    return true;
}

bool FMazePipeFitting::FitSleeve(const FTransform& StraightTransform, const FMazePipeSourceGeometry& StraightGeometry,
    const FMazePipeSleeve& Sleeve, float TargetDiameter, FTransform& OutTransform, FString& OutError)
{
    if (StraightGeometry.Piece != EMazePipePiece::Straight || Sleeve.Direction < 0 || Sleeve.Direction > 3 || Sleeve.Center.ContainsNaN())
    {
        OutError = TEXT("A connector sleeve needs a straight pipe and a valid direction.");
        return false;
    }
    FTransform Fitted;
    if (!FitMesh(StraightTransform, StraightGeometry, TargetDiameter, Sleeve.Length, Fitted, OutError))
    {
        return false;
    }
    const FQuat Turn(FVector(0, 0, 1), FMath::DegreesToRadians(-90.0 * Sleeve.Direction));
    OutTransform = FTransform(Turn * Fitted.GetRotation(), Sleeve.Center + Turn.RotateVector(Fitted.GetTranslation()), Fitted.GetScale3D());
    return true;
}

void FMazePipeFitting::GetCellAxes(const FMazePipeGridFrame& Frame, int32 Rotation, bool bWallConnection, FVector& OutX, FVector& OutY)
{
    const int32 Turn = ((Rotation % 4) + 4) % 4;
    const FVector Directions[] = { Frame.Up, Frame.Right, -Frame.Up, -Frame.Right };
    OutX = bWallConnection ? -Frame.Inward : Directions[(Turn + 1) % 4];
    OutY = Directions[Turn];
}