// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MazePipePuzzle.generated.h"

UENUM(BlueprintType)
enum class EMazePipePiece : uint8
{
    Empty,
    Straight,
    Bend,
    Tee,
    Cross,
    Chamber,
    Inlet,
    Outlet
};

UENUM(BlueprintType)
enum class EMazePipeFlowLightState : uint8
{
    Idle,
    Flowing,
    Leak,
    Completed
};

UENUM(BlueprintType)
enum class EMazePipeChamberLightState : uint8
{
    Required,
    Connected,
    Bypassed
};

USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeGridSettings
{
    GENERATED_BODY()

    // Add enough columns to span the wall width while keeping square cells.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes")
    bool bFitColumnsToWall = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "1", ClampMax = "32", EditCondition = "!bFitColumnsToWall"))
    int32 Columns = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "1", ClampMax = "32"))
    int32 Rows = 4;

    // Includes the inlet, chamber and main outlet cells.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "5", ClampMax = "128"))
    int32 MinimumPathCells = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "5", ClampMax = "128"))
    int32 MaximumPathCells = 24;

    // Minimum horizontal plus vertical grid distance between any two locked endpoints.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "1"))
    int32 MinimumEndpointDistance = 4;

    // Additional required outlets branch from the route after chamber 004.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "0", ClampMax = "8"))
    int32 ExtraOutletCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "1", ClampMax = "256"))
    int32 GenerationAttempts = 64;

    // Limits DFS work per attempt, including backtracking.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipes", meta = (ClampMin = "100", ClampMax = "100000"))
    int32 SearchNodeBudget = 10000;
};

USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeCell
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    EMazePipePiece Piece = EMazePipePiece::Empty;

    // Clockwise turns as viewed from inside the room facing this wall.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 Rotation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 SolutionRotation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    bool bLocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    bool bOnSolution = false;

    // Zero is on the planned solution. Larger values are grid distance from it.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 DistanceFromSolution = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeLayout
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 Columns = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 Rows = 0;

    // Index is Row * Columns + Column. Row zero is the bottom row.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<FMazePipeCell> Cells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 InletIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 ChamberIndex = INDEX_NONE;

    // The first entry is the main outlet. Every entry must receive steam.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<int32> RequiredOutlets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<int32> MainPath;
};

USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeLeak
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 CellIndex = INDEX_NONE;

    // Zero is up, one is right, two is down and three is left.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    int32 Direction = 0;
};

USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeSteamResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    bool bPassed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    bool bChamberBypassed = false;

    // BFS distance from the inlet. INDEX_NONE means this cell received no steam.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<int32> SteamDistances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<FMazePipeLeak> Leaks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipes")
    TArray<int32> MissingOutlets;
};

struct AM_2610_API FMazePipeGridFrame
{
    FVector BottomLeft = FVector::ZeroVector;
    FVector Right = FVector::RightVector;
    FVector Up = FVector::UpVector;
    FVector Inward = FVector::ForwardVector;
    float CellSize = 0.0f;
    int32 Columns = 0;
    int32 Rows = 0;

    FVector GetCellCenter(int32 CellIndex) const;
};

// This helper stores no actors and does not add another component to the room.
class AM_2610_API FMazePipePuzzle
{
public:
    // Corners are bottom left, bottom right, top left and top right, in that order.
    static bool BuildGrid(const TArray<FVector>& Corners, const FVector& TowardRoom, int32 Columns, int32 Rows,
        float BorderPadding, float WallOffset, FMazePipeGridFrame& OutFrame, FString& OutError, bool bFitColumnsToWall = false);
    static bool Generate(const FMazePipeGridSettings& Settings, int32 Seed, FMazePipeLayout& OutLayout, FString& OutError);
    static bool MakeFailedPreview(const FMazePipeLayout& SourceLayout, FMazePipeLayout& OutLayout, int32& ChangedCell, FString& OutError);
    static bool RotateCell(FMazePipeLayout& Layout, int32 CellIndex, bool bRotateRight);
    static void RestoreSolution(FMazePipeLayout& Layout);
    static FMazePipeSteamResult TraceSteam(const FMazePipeLayout& Layout);
    static int32 GetLastSteamLightStep(const FMazePipeSteamResult& Result);
    static bool IsSteamCheckFinished(const FMazePipeSteamResult& Result, int32 Step);
    static bool AdvanceSteamCompletion(const FMazePipeSteamResult& Result, int32 Step, bool bPreviouslyCompleted);
    static EMazePipeFlowLightState GetSteamLightState(const FMazePipeSteamResult& Result, int32 CellIndex, int32 Step, bool bCompleted = false);
    static EMazePipeChamberLightState GetChamberLightState(const FMazePipeLayout& Layout,
        const FMazePipeSteamResult& Result, int32 Step, bool bFinished);
    static uint8 GetPorts(const FMazePipeCell& Cell);
    static int32 GetNeighbor(const FMazePipeLayout& Layout, int32 CellIndex, int32 Direction);
};