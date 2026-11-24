// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipePuzzle.h"

namespace
{
    uint8 RotatePorts(uint8 Ports, int32 Rotation)
    {
        const int32 Turns = ((Rotation % 4) + 4) % 4;
        return static_cast<uint8>(((Ports << Turns) | (Ports >> (4 - Turns))) & 15);
    }

    uint8 BasePorts(EMazePipePiece Piece)
    {
        switch (Piece)
        {
        case EMazePipePiece::Straight:
        case EMazePipePiece::Chamber:
            return 5;
        case EMazePipePiece::Bend:
            return 3;
        case EMazePipePiece::Tee:
            return 11;
        case EMazePipePiece::Cross:
            return 15;
        case EMazePipePiece::Inlet:
        case EMazePipePiece::Outlet:
            // The other opening faces the wall and is an external connection.
            return 1;
        default:
            return 0;
        }
    }

    void Shuffle(TArray<int32>& Values, FRandomStream& Random)
    {
        for (int32 Index = Values.Num() - 1; Index > 0; --Index)
        {
            Values.Swap(Index, Random.RandRange(0, Index));
        }
    }

    int32 GridDistance(const FMazePipeLayout& Layout, int32 First, int32 Second)
    {
        return FMath::Abs(First % Layout.Columns - Second % Layout.Columns)
            + FMath::Abs(First / Layout.Columns - Second / Layout.Columns);
    }

    int32 DirectionTo(const FMazePipeLayout& Layout, int32 First, int32 Second)
    {
        for (int32 Direction = 0; Direction < 4; ++Direction)
        {
            if (FMazePipePuzzle::GetNeighbor(Layout, First, Direction) == Second)
            {
                return Direction;
            }
        }
        return INDEX_NONE;
    }

    bool HasChamberClearance(const FMazePipeLayout& Layout, int32 Chamber, int32 Endpoint)
    {
        const int32 ColumnsApart = FMath::Abs(Chamber % Layout.Columns - Endpoint % Layout.Columns);
        const int32 RowsApart = FMath::Abs(Chamber / Layout.Columns - Endpoint / Layout.Columns);
        return ColumnsApart > 1 || RowsApart > 1;
    }

    bool IsChamberCandidate(const FMazePipeLayout& Layout, const TArray<int32>& Path, int32 Position)
    {
        const int32 Chamber = Path[Position];
        const int32 Before = DirectionTo(Layout, Chamber, Path[Position - 1]);
        const int32 After = DirectionTo(Layout, Chamber, Path[Position + 1]);
        return Before == (After + 2) % 4 && HasChamberClearance(Layout, Chamber, Path[0])
            && HasChamberClearance(Layout, Chamber, Path.Last());
    }

    bool HasChamberSlot(const FMazePipeLayout& Layout, const TArray<int32>& Path)
    {
        for (int32 Position = 2; Position < Path.Num() - 2; ++Position)
        {
            if (IsChamberCandidate(Layout, Path, Position))
            {
                return true;
            }
        }
        return false;
    }

    bool FindPath(const FMazePipeGridSettings& Settings, const FMazePipeLayout& Layout, int32 Goal,
        FRandomStream& Random, TArray<uint8>& Visited, TArray<int32>& Path, int32& RemainingSearch, bool bNeedsChamber = true)
    {
        if (--RemainingSearch < 0)
        {
            return false;
        }

        const int32 Current = Path.Last();
        if (Current == Goal)
        {
            return Path.Num() >= Settings.MinimumPathCells
                && (!bNeedsChamber || HasChamberSlot(Layout, Path));
        }

        if (Path.Num() >= Settings.MaximumPathCells
            || Path.Num() + GridDistance(Layout, Current, Goal) > Settings.MaximumPathCells)
        {
            return false;
        }

        TArray<int32> Directions = { 0, 1, 2, 3 };
        Shuffle(Directions, Random);
        for (int32 Direction : Directions)
        {
            const int32 Next = FMazePipePuzzle::GetNeighbor(Layout, Current, Direction);
            if (Next == INDEX_NONE || Visited[Next] || RemainingSearch <= 0)
            {
                continue;
            }

            Visited[Next] = 1;
            Path.Add(Next);
            if (FindPath(Settings, Layout, Goal, Random, Visited, Path, RemainingSearch, bNeedsChamber))
            {
                return true;
            }
            Path.RemoveAt(Path.Num() - 1);
            Visited[Next] = 0;
        }
        return false;
    }

    void AddEdge(const FMazePipeLayout& Layout, TArray<uint8>& Masks, int32 First, int32 Second)
    {
        const int32 Direction = DirectionTo(Layout, First, Second);
        check(Direction != INDEX_NONE);
        Masks[First] |= 1 << Direction;
        Masks[Second] |= 1 << ((Direction + 2) % 4);
    }

    bool HasEndpointClearance(const FMazePipeGridSettings& Settings, const FMazePipeLayout& Layout, int32 CellIndex)
    {
        if (GridDistance(Layout, Layout.InletIndex, CellIndex) < Settings.MinimumEndpointDistance
            || !HasChamberClearance(Layout, Layout.ChamberIndex, CellIndex))
        {
            return false;
        }
        for (int32 Outlet : Layout.RequiredOutlets)
        {
            if (GridDistance(Layout, Outlet, CellIndex) < Settings.MinimumEndpointDistance)
            {
                return false;
            }
        }
        return true;
    }

    bool AddBranches(const FMazePipeGridSettings& Settings, FMazePipeLayout& Layout,
        TArray<uint8>& Masks, int32 ChamberPosition, FRandomStream& Random, int32& RemainingSearch)
    {
        TArray<int32> Origins;
        for (int32 Index = ChamberPosition + 1; Index < Layout.MainPath.Num() - 1; ++Index)
        {
            Origins.Add(Layout.MainPath[Index]);
        }

        for (int32 Branch = 0; Branch < Settings.ExtraOutletCount; ++Branch)
        {
            TArray<int32> Goals;
            for (int32 Index = 0; Index < Layout.Cells.Num(); ++Index)
            {
                if (Masks[Index] == 0 && HasEndpointClearance(Settings, Layout, Index))
                {
                    Goals.Add(Index);
                }
            }
            if (Goals.IsEmpty())
            {
                return false;
            }
            Shuffle(Goals, Random);
            TArray<int32> Candidates = Origins;
            Shuffle(Candidates, Random);
            bool bAddedBranch = false;
            FMazePipeGridSettings BranchSettings = Settings;
            BranchSettings.MinimumPathCells = 2;
            BranchSettings.MaximumPathCells = FMath::Min(Settings.MaximumPathCells, 12);
            for (int32 Origin : Candidates)
            {
                for (int32 Goal : Goals)
                {
                    if (RemainingSearch <= 0)
                    {
                        return false;
                    }
                    if (GridDistance(Layout, Origin, Goal) + 1 > BranchSettings.MaximumPathCells)
                    {
                        continue;
                    }
                    TArray<uint8> Visited;
                    Visited.Init(0, Layout.Cells.Num());
                    for (int32 Index = 0; Index < Masks.Num(); ++Index)
                    {
                        Visited[Index] = Masks[Index] != 0;
                    }
                    TArray<int32> Path;
                    Path.Add(Origin);
                    if (!FindPath(BranchSettings, Layout, Goal, Random, Visited, Path, RemainingSearch, false))
                    {
                        continue;
                    }
                    for (int32 Position = 1; Position < Path.Num(); ++Position)
                    {
                        AddEdge(Layout, Masks, Path[Position - 1], Path[Position]);
                        if (Position < Path.Num() - 1)
                        {
                            Origins.Add(Path[Position]);
                        }
                    }
                    Layout.RequiredOutlets.Add(Goal);
                    bAddedBranch = true;
                    break;
                }
                if (bAddedBranch)
                {
                    break;
                }
            }
            if (!bAddedBranch)
            {
                return false;
            }
        }
        return true;
    }

    void AddOptionalFourWayLoop(const FMazePipeLayout& Layout, TArray<uint8>& Masks, FRandomStream Random)
    {
        const uint8 AllPorts = BasePorts(EMazePipePiece::Cross);
        if (Masks.Contains(AllPorts) || Random.RandRange(0, 2) == 0)
        {
            return;
        }

        const int32 FirstCell = Random.RandRange(0, Masks.Num() - 1);
        for (int32 Offset = 0; Offset < Masks.Num(); ++Offset)
        {
            const int32 Junction = (FirstCell + Offset) % Masks.Num();
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                const int32 Adjacent = (Direction + 1) % 4;
                const int32 LoopPorts = (1 << Direction) | (1 << Adjacent);
                if (Masks[Junction] != (AllPorts ^ LoopPorts))
                {
                    continue;
                }

                const int32 First = FMazePipePuzzle::GetNeighbor(Layout, Junction, Direction);
                const int32 Last = FMazePipePuzzle::GetNeighbor(Layout, Junction, Adjacent);
                const int32 Corner = FMazePipePuzzle::GetNeighbor(Layout, First, Adjacent);
                if (First == INDEX_NONE || Last == INDEX_NONE || Corner == INDEX_NONE
                    || Masks[First] != 0 || Masks[Last] != 0 || Masks[Corner] != 0)
                {
                    continue;
                }

                // This loop shares one junction with the route, keeping 004 mandatory.
                AddEdge(Layout, Masks, Junction, First);
                AddEdge(Layout, Masks, First, Corner);
                AddEdge(Layout, Masks, Corner, Last);
                AddEdge(Layout, Masks, Last, Junction);
                return;
            }
        }
    }

    bool AssignPiece(FMazePipeCell& Cell, uint8 Mask)
    {
        for (int32 Rotation = 0; Rotation < 4; ++Rotation)
        {
            if (RotatePorts(BasePorts(Cell.Piece), Rotation) == Mask)
            {
                Cell.Rotation = Rotation;
                Cell.SolutionRotation = Rotation;
                return true;
            }
        }
        return false;
    }

    void SetSolutionDistances(FMazePipeLayout& Layout)
    {
        TArray<int32> Queue;
        for (int32 Index = 0; Index < Layout.Cells.Num(); ++Index)
        {
            Layout.Cells[Index].DistanceFromSolution = Layout.Cells[Index].bOnSolution ? 0 : INDEX_NONE;
            if (Layout.Cells[Index].bOnSolution)
            {
                Queue.Add(Index);
            }
        }

        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const int32 Current = Queue[Head];
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                const int32 Next = FMazePipePuzzle::GetNeighbor(Layout, Current, Direction);
                if (Next != INDEX_NONE && Layout.Cells[Next].DistanceFromSolution == INDEX_NONE)
                {
                    Layout.Cells[Next].DistanceFromSolution = Layout.Cells[Current].DistanceFromSolution + 1;
                    Queue.Add(Next);
                }
            }
        }
    }

    void TraceConnections(const FMazePipeLayout& Layout, int32 BlockedCell,
        TArray<int32>& Distances, TArray<FMazePipeLeak>* Leaks)
    {
        Distances.Init(INDEX_NONE, Layout.Cells.Num());
        TArray<int32> Queue;
        Queue.Add(Layout.InletIndex);
        Distances[Layout.InletIndex] = 0;

        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const int32 Current = Queue[Head];
            const uint8 Ports = FMazePipePuzzle::GetPorts(Layout.Cells[Current]);
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                if (!(Ports & (1 << Direction)))
                {
                    continue;
                }

                const int32 Next = FMazePipePuzzle::GetNeighbor(Layout, Current, Direction);
                if (Next == BlockedCell && BlockedCell != INDEX_NONE)
                {
                    continue;
                }
                if (Next == INDEX_NONE || !(FMazePipePuzzle::GetPorts(Layout.Cells[Next]) & (1 << ((Direction + 2) % 4))))
                {
                    if (Leaks)
                    {
                        FMazePipeLeak Leak;
                        Leak.CellIndex = Current;
                        Leak.Direction = Direction;
                        Leaks->Add(Leak);
                    }
                    continue;
                }

                if (Distances[Next] == INDEX_NONE)
                {
                    Distances[Next] = Distances[Current] + 1;
                    Queue.Add(Next);
                }
            }
        }
    }
}

FVector FMazePipeGridFrame::GetCellCenter(int32 CellIndex) const
{
    check(Columns > 0 && Rows > 0 && CellIndex >= 0 && CellIndex < Columns * Rows);
    return BottomLeft + Right * ((CellIndex % Columns + 0.5f) * CellSize)
        + Up * ((CellIndex / Columns + 0.5f) * CellSize);
}

bool FMazePipePuzzle::BuildGrid(const TArray<FVector>& Corners, const FVector& TowardRoom, int32 Columns, int32 Rows,
    float BorderPadding, float WallOffset, FMazePipeGridFrame& OutFrame, FString& OutError, bool bFitColumnsToWall)
{
    OutFrame = FMazePipeGridFrame();
    OutError.Reset();
    if (Corners.Num() != 4 || Rows < 1 || Rows > 32
        || (!bFitColumnsToWall && (Columns < 1 || Columns > 32 || Columns * Rows > 512))
        || !FMath::IsFinite(BorderPadding) || BorderPadding < 0.0f
        || !FMath::IsFinite(WallOffset) || WallOffset < 0.0f || TowardRoom.ContainsNaN())
    {
        OutError = TEXT("The pipe grid needs four valid corners, valid dimensions and nonnegative padding and wall offset.");
        return false;
    }
    for (const FVector& Corner : Corners)
    {
        if (Corner.ContainsNaN())
        {
            OutError = TEXT("A pipe grid corner has an invalid position.");
            return false;
        }
    }

    const FVector Horizontal = Corners[1] - Corners[0];
    const FVector Vertical = Corners[2] - Corners[0];
    const double Width = Horizontal.Size();
    const double Height = Vertical.Size();
    if (!FMath::IsFinite(Width) || !FMath::IsFinite(Height)
        || Width <= 2.0 * BorderPadding + 1.0 || Height <= 2.0 * BorderPadding + 1.0)
    {
        OutError = TEXT("The pipe rectangle has no usable space after border padding.");
        return false;
    }

    const FVector Right = Horizontal / Width;
    const FVector Up = Vertical / Height;
    const double Tolerance = FMath::Max(0.1, FMath::Max(Width, Height) * 0.001);
    if (FMath::Abs(FVector::DotProduct(Right, Up)) > 0.001
        || FVector::Dist(Corners[3], Corners[1] + Vertical) > Tolerance)
    {
        OutError = TEXT("The four pipe arrows must form a flat rectangle with perpendicular sides. Check the arrow base positions.");
        return false;
    }

    FVector Inward = FVector::CrossProduct(Right, Up).GetSafeNormal();
    const double Facing = FVector::DotProduct(Inward, TowardRoom);
    if (FMath::Abs(Facing) < 0.01)
    {
        OutError = TEXT("The opposite pipe wall must be across the room so the wall offset can face inward.");
        return false;
    }
    if (Facing < 0.0)
    {
        Inward *= -1.0;
    }

    if (bFitColumnsToWall)
    {
        const double DesiredColumns = (Width - 2.0 * BorderPadding) * Rows / (Height - 2.0 * BorderPadding);
        if (DesiredColumns > 32.000001)
        {
            OutError = TEXT("The wall needs more than 32 columns to fit square cells. Reduce Rows or narrow the arrow rectangle.");
            return false;
        }
        Columns = FMath::Max(1, FMath::CeilToInt32(DesiredColumns - 0.000001));
        if (Columns * Rows > 512)
        {
            OutError = TEXT("The fitted pipe grid exceeds 512 cells. Reduce Rows or narrow the arrow rectangle.");
            return false;
        }
    }

    OutFrame.CellSize = static_cast<float>(FMath::Min((Width - 2.0 * BorderPadding) / Columns, (Height - 2.0 * BorderPadding) / Rows));
    OutFrame.Right = Right;
    OutFrame.Up = Up;
    OutFrame.Inward = Inward;
    OutFrame.Columns = Columns;
    OutFrame.Rows = Rows;
    OutFrame.BottomLeft = Corners[0] + Right * ((Width - Columns * OutFrame.CellSize) * 0.5)
        + Up * ((Height - Rows * OutFrame.CellSize) * 0.5) + Inward * WallOffset;
    return true;
}

uint8 FMazePipePuzzle::GetPorts(const FMazePipeCell& Cell)
{
    return RotatePorts(BasePorts(Cell.Piece), Cell.Rotation);
}

int32 FMazePipePuzzle::GetNeighbor(const FMazePipeLayout& Layout, int32 CellIndex, int32 Direction)
{
    if (Layout.Columns < 1 || Layout.Rows < 1 || !Layout.Cells.IsValidIndex(CellIndex) || Direction < 0 || Direction > 3)
    {
        return INDEX_NONE;
    }

    const int32 Column = CellIndex % Layout.Columns;
    const int32 Row = CellIndex / Layout.Columns;
    const int32 NextColumn = Column + (Direction == 1 ? 1 : Direction == 3 ? -1 : 0);
    const int32 NextRow = Row + (Direction == 0 ? 1 : Direction == 2 ? -1 : 0);
    if (NextColumn < 0 || NextColumn >= Layout.Columns || NextRow < 0 || NextRow >= Layout.Rows)
    {
        return INDEX_NONE;
    }
    const int32 Next = NextRow * Layout.Columns + NextColumn;
    return Layout.Cells.IsValidIndex(Next) ? Next : INDEX_NONE;
}

bool FMazePipePuzzle::Generate(const FMazePipeGridSettings& Settings, int32 Seed, FMazePipeLayout& OutLayout, FString& OutError)
{
    OutLayout = FMazePipeLayout();
    OutError.Reset();
    if (Settings.Columns < 1 || Settings.Columns > 32 || Settings.Rows < 1 || Settings.Rows > 32
        || Settings.Columns * Settings.Rows > 512 || Settings.MinimumPathCells < 5
        || Settings.MaximumPathCells < Settings.MinimumPathCells || Settings.MaximumPathCells > 128
        || Settings.MaximumPathCells > Settings.Columns * Settings.Rows || Settings.MinimumEndpointDistance < 1
        || Settings.MinimumEndpointDistance > Settings.Columns + Settings.Rows - 2
        || Settings.ExtraOutletCount < 0 || Settings.ExtraOutletCount > 8
        || Settings.MinimumPathCells + Settings.ExtraOutletCount > Settings.Columns * Settings.Rows
        || Settings.GenerationAttempts < 1 || Settings.GenerationAttempts > 256
        || Settings.SearchNodeBudget < 100 || Settings.SearchNodeBudget > 100000)
    {
        OutError = TEXT("Check the pipe grid dimensions, route lengths, endpoint distance and search budgets.");
        return false;
    }

    FRandomStream Random(Seed);
    for (int32 Attempt = 0; Attempt < Settings.GenerationAttempts; ++Attempt)
    {
        FMazePipeLayout Layout;
        Layout.Columns = Settings.Columns;
        Layout.Rows = Settings.Rows;
        const int32 Count = Settings.Columns * Settings.Rows;
        Layout.Cells.SetNum(Count);
        Layout.InletIndex = Random.RandRange(0, Count - 1);

        TArray<int32> Goals;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (GridDistance(Layout, Layout.InletIndex, Index) >= Settings.MinimumEndpointDistance)
            {
                Goals.Add(Index);
            }
        }
        if (Goals.IsEmpty())
        {
            continue;
        }
        const int32 Goal = Goals[Random.RandRange(0, Goals.Num() - 1)];
        TArray<uint8> Visited;
        Visited.Init(0, Count);
        Visited[Layout.InletIndex] = 1;
        Layout.MainPath.Add(Layout.InletIndex);
        int32 RemainingSearch = Settings.SearchNodeBudget;
        if (!FindPath(Settings, Layout, Goal, Random, Visited, Layout.MainPath, RemainingSearch))
        {
            continue;
        }

        TArray<int32> ChamberChoices;
        for (int32 Position = 2; Position < Layout.MainPath.Num() - 2; ++Position)
        {
            if (IsChamberCandidate(Layout, Layout.MainPath, Position))
            {
                ChamberChoices.Add(Position);
            }
        }
        Shuffle(ChamberChoices, Random);

        bool bBuiltSolution = false;
        TArray<uint8> Masks;
        for (int32 ChamberPosition : ChamberChoices)
        {
            Masks.Init(0, Count);
            Layout.RequiredOutlets.Reset();
            Layout.RequiredOutlets.Add(Goal);
            Layout.ChamberIndex = Layout.MainPath[ChamberPosition];
            for (int32 Position = 1; Position < Layout.MainPath.Num(); ++Position)
            {
                AddEdge(Layout, Masks, Layout.MainPath[Position - 1], Layout.MainPath[Position]);
            }
            if (AddBranches(Settings, Layout, Masks, ChamberPosition, Random, RemainingSearch))
            {
                // The optional shape uses a copy of the board's random stream.
                AddOptionalFourWayLoop(Layout, Masks, Random);
                bBuiltSolution = true;
                break;
            }
        }
        if (!bBuiltSolution)
        {
            continue;
        }

        for (int32 Index = 0; Index < Count; ++Index)
        {
            FMazePipeCell& Cell = Layout.Cells[Index];
            const uint8 Mask = Masks[Index];
            Cell.bOnSolution = Mask != 0;
            if (Mask == 0)
            {
                const EMazePipePiece Decoys[] = { EMazePipePiece::Straight, EMazePipePiece::Bend, EMazePipePiece::Tee, EMazePipePiece::Cross };
                Cell.Piece = Decoys[Random.RandRange(0, 3)];
                Cell.Rotation = Random.RandRange(0, 3);
                Cell.SolutionRotation = Cell.Rotation;
                continue;
            }

            Cell.bLocked = Index == Layout.InletIndex || Layout.RequiredOutlets.Contains(Index);
            if (Index == Layout.InletIndex)
            {
                Cell.Piece = EMazePipePiece::Inlet;
            }
            else if (Layout.RequiredOutlets.Contains(Index))
            {
                Cell.Piece = EMazePipePiece::Outlet;
            }
            else if (Index == Layout.ChamberIndex)
            {
                Cell.Piece = EMazePipePiece::Chamber;
            }
            else
            {
                const int32 PortCount = !!(Mask & 1) + !!(Mask & 2) + !!(Mask & 4) + !!(Mask & 8);
                Cell.Piece = PortCount == 4 ? EMazePipePiece::Cross : PortCount == 3 ? EMazePipePiece::Tee
                    : (Mask == 5 || Mask == 10) ? EMazePipePiece::Straight : EMazePipePiece::Bend;
            }
            if (!AssignPiece(Cell, Mask))
            {
                OutError = TEXT("A generated pipe connection did not match a supported piece shape.");
                return false;
            }
        }

        SetSolutionDistances(Layout);
        if (!TraceSteam(Layout).bPassed)
        {
            continue;
        }

        for (FMazePipeCell& Cell : Layout.Cells)
        {
            if (!Cell.bLocked)
            {
                Cell.Rotation = Random.RandRange(0, 3);
            }
        }
        if (TraceSteam(Layout).bPassed)
        {
            const int32 FirstPipe = Layout.MainPath[1];
            const int32 TowardInlet = DirectionTo(Layout, FirstPipe, Layout.InletIndex);
            for (int32 Turn = 0; Turn < 4 && (GetPorts(Layout.Cells[FirstPipe]) & (1 << TowardInlet)); ++Turn)
            {
                RotateCell(Layout, FirstPipe, true);
            }
        }
        if (TraceSteam(Layout).bPassed)
        {
            continue;
        }
        OutLayout = MoveTemp(Layout);
        return true;
    }

    OutError = TEXT("No pipe layout fitted the current constraints within the search budget. Adjust the grid, route lengths or extra outlet count.");
    return false;
}

bool FMazePipePuzzle::MakeFailedPreview(const FMazePipeLayout& SourceLayout, FMazePipeLayout& OutLayout, int32& ChangedCell, FString& OutError)
{
    FMazePipeLayout Solved = SourceLayout;
    OutLayout = FMazePipeLayout();
    ChangedCell = INDEX_NONE;
    OutError.Reset();
    RestoreSolution(Solved);
    if (Solved.MainPath.Num() < 3 || Solved.MainPath[0] != Solved.InletIndex || !TraceSteam(Solved).bPassed)
    {
        OutError = TEXT("Generate a valid pipe layout before creating a failed preview.");
        return false;
    }

    // Prefer a later break so the preview shows steam travelling before it leaks.
    for (int32 Position = Solved.MainPath.Num() - 2; Position > 0; --Position)
    {
        const int32 Index = Solved.MainPath[Position];
        if (!Solved.Cells.IsValidIndex(Index) || Solved.Cells[Index].bLocked || Index == Solved.ChamberIndex
            || Solved.Cells[Index].Piece == EMazePipePiece::Cross)
        {
            continue;
        }
        FMazePipeLayout Candidate = Solved;
        for (int32 Turn = 0; Turn < 3; ++Turn)
        {
            if (!RotateCell(Candidate, Index, true))
            {
                break;
            }
            const FMazePipeSteamResult Result = TraceSteam(Candidate);
            if (!Result.bPassed && !Result.Leaks.IsEmpty() && !Result.MissingOutlets.IsEmpty()
                && GetLastSteamLightStep(Result) >= 3)
            {
                ChangedCell = Index;
                OutLayout = MoveTemp(Candidate);
                return true;
            }
        }
    }

    // A generated layout always has a rotatable piece immediately after its inlet.
    const int32 FirstPipe = Solved.MainPath[1];
    const int32 TowardInlet = DirectionTo(Solved, FirstPipe, Solved.InletIndex);
    if (TowardInlet != INDEX_NONE)
    {
        for (int32 Turn = 0; Turn < 3; ++Turn)
        {
            if (!RotateCell(Solved, FirstPipe, true))
            {
                break;
            }
            if (!(GetPorts(Solved.Cells[FirstPipe]) & (1 << TowardInlet)))
            {
                const FMazePipeSteamResult Result = TraceSteam(Solved);
                if (!Result.bPassed && !Result.Leaks.IsEmpty() && !Result.MissingOutlets.IsEmpty())
                {
                    ChangedCell = FirstPipe;
                    OutLayout = MoveTemp(Solved);
                    return true;
                }
            }
        }
    }
    OutError = TEXT("Could not create a leaking preview from this pipe layout.");
    return false;
}

bool FMazePipePuzzle::RotateCell(FMazePipeLayout& Layout, int32 CellIndex, bool bRotateRight)
{
    if (!Layout.Cells.IsValidIndex(CellIndex) || Layout.Cells[CellIndex].bLocked || Layout.Cells[CellIndex].Piece == EMazePipePiece::Empty)
    {
        return false;
    }
    FMazePipeCell& Cell = Layout.Cells[CellIndex];
    Cell.Rotation = (Cell.Rotation + (bRotateRight ? 1 : 3)) % 4;
    return true;
}

void FMazePipePuzzle::RestoreSolution(FMazePipeLayout& Layout)
{
    for (FMazePipeCell& Cell : Layout.Cells)
    {
        Cell.Rotation = Cell.SolutionRotation;
    }
}

FMazePipeSteamResult FMazePipePuzzle::TraceSteam(const FMazePipeLayout& Layout)
{
    FMazePipeSteamResult Result;
    if (Layout.Columns < 1 || Layout.Columns > 32 || Layout.Rows < 1 || Layout.Rows > 32
        || Layout.Cells.Num() != Layout.Columns * Layout.Rows || !Layout.Cells.IsValidIndex(Layout.InletIndex)
        || !Layout.Cells.IsValidIndex(Layout.ChamberIndex) || Layout.RequiredOutlets.IsEmpty()
        || Layout.Cells[Layout.InletIndex].Piece != EMazePipePiece::Inlet
        || Layout.Cells[Layout.ChamberIndex].Piece != EMazePipePiece::Chamber)
    {
        return Result;
    }

    TraceConnections(Layout, INDEX_NONE, Result.SteamDistances, &Result.Leaks);
    TArray<int32> WithoutChamber;
    TraceConnections(Layout, Layout.ChamberIndex, WithoutChamber, nullptr);
    for (int32 Outlet : Layout.RequiredOutlets)
    {
        if (!Layout.Cells.IsValidIndex(Outlet) || Layout.Cells[Outlet].Piece != EMazePipePiece::Outlet
            || Result.SteamDistances[Outlet] == INDEX_NONE)
        {
            Result.MissingOutlets.Add(Outlet);
        }
        if (WithoutChamber.IsValidIndex(Outlet) && WithoutChamber[Outlet] != INDEX_NONE)
        {
            Result.bChamberBypassed = true;
        }
    }
    Result.bPassed = Result.MissingOutlets.IsEmpty() && Result.Leaks.IsEmpty()
        && Result.SteamDistances[Layout.ChamberIndex] != INDEX_NONE && !Result.bChamberBypassed;
    return Result;
}

int32 FMazePipePuzzle::GetLastSteamLightStep(const FMazePipeSteamResult& Result)
{
    int32 Last = INDEX_NONE;
    for (int32 Distance : Result.SteamDistances)
    {
        Last = FMath::Max(Last, Distance);
    }
    // Let steam reach an opening before displaying its leak.
    return Last == INDEX_NONE ? INDEX_NONE : Last + 1;
}

bool FMazePipePuzzle::IsSteamCheckFinished(const FMazePipeSteamResult& Result, int32 Step)
{
    const int32 Last = GetLastSteamLightStep(Result);
    return Last != INDEX_NONE && Step >= Last;
}

bool FMazePipePuzzle::AdvanceSteamCompletion(const FMazePipeSteamResult& Result, int32 Step, bool bPreviouslyCompleted)
{
    return bPreviouslyCompleted || (Result.bPassed && IsSteamCheckFinished(Result, Step));
}

EMazePipeFlowLightState FMazePipePuzzle::GetSteamLightState(const FMazePipeSteamResult& Result, int32 CellIndex, int32 Step, bool bCompleted)
{
    if (Step < 0 || !Result.SteamDistances.IsValidIndex(CellIndex)
        || Result.SteamDistances[CellIndex] < 0)
    {
        return EMazePipeFlowLightState::Idle;
    }
    if (bCompleted && Result.bPassed)
    {
        return EMazePipeFlowLightState::Completed;
    }
    if (Result.SteamDistances[CellIndex] > Step)
    {
        return EMazePipeFlowLightState::Idle;
    }
    for (const FMazePipeLeak& Leak : Result.Leaks)
    {
        if (Leak.CellIndex == CellIndex && Step > Result.SteamDistances[CellIndex])
        {
            return EMazePipeFlowLightState::Leak;
        }
    }
    return EMazePipeFlowLightState::Flowing;
}

EMazePipeChamberLightState FMazePipePuzzle::GetChamberLightState(const FMazePipeLayout& Layout,
    const FMazePipeSteamResult& Result, int32 Step, bool bFinished)
{
    if (bFinished && Result.bChamberBypassed)
    {
        return EMazePipeChamberLightState::Bypassed;
    }
    if (Result.SteamDistances.IsValidIndex(Layout.ChamberIndex)
        && Result.SteamDistances[Layout.ChamberIndex] >= 0 && Result.SteamDistances[Layout.ChamberIndex] <= Step)
    {
        return EMazePipeChamberLightState::Connected;
    }
    return EMazePipeChamberLightState::Required;
}