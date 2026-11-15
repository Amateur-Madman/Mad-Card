// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enviornment/MazePipePuzzle.h"

struct AM_2610_API FMazePipeSourceGeometry
{
    EMazePipePiece Piece = EMazePipePiece::Empty;
    FVector Center = FVector::ZeroVector;
    TArray<float> PortDistances;
    float Diameter = 0.0f;
    uint8 Ports = 0;
};

struct AM_2610_API FMazePipeSleeve
{
    FVector Center = FVector::ZeroVector;
    float Length = 0.0f;
    int32 Direction = 0;
};

// Connector positions are measured in the pipe Blueprint's local XY plane.
class AM_2610_API FMazePipeFitting
{
public:
    static bool ReadSource(EMazePipePiece Piece, const TArray<FVector>& PortPositions, float Diameter,
        FMazePipeSourceGeometry& OutGeometry, FString& OutError);
    static bool FitMesh(const FTransform& AuthoredTransform, const FMazePipeSourceGeometry& Geometry,
        float TargetDiameter, float TargetLength, FTransform& OutTransform, FString& OutError);
    static bool BuildSleeves(const FMazePipeSourceGeometry& Geometry, float CellSize, float TargetDiameter,
        bool bWallConnection, float WallReach, TArray<FMazePipeSleeve>& OutSleeves, FString& OutError);
    static bool FitSleeve(const FTransform& StraightTransform, const FMazePipeSourceGeometry& StraightGeometry,
        const FMazePipeSleeve& Sleeve, float TargetDiameter, FTransform& OutTransform, FString& OutError);
    static void GetCellAxes(const FMazePipeGridFrame& Frame, int32 Rotation, bool bWallConnection, FVector& OutX, FVector& OutY);
};