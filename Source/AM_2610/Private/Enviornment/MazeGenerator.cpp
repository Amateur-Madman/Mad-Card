// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeActors.h"
#include "Enviornment/MazeInterface.h"
#include "Enviornment/MazePuzzleComponent.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "AM_2610/AM_2610.h"
#include "Engine/World.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
    // Sort the connection arrows by name for repeatable random choices.
    TArray<UArrowComponent*> SortedConnections(AMazeActors* Piece)
    {
        if (!IsValid(Piece))
        {
            return {};
        }

        TArray<UArrowComponent*> Connections = Piece->GetConnections();

        Connections.RemoveAll([](UArrowComponent* Arrow)
            {
                return !IsValid(Arrow) || Arrow->ComponentHasTag(TEXT("EnemySpawn")) || Arrow->ComponentHasTag(TEXT("PlayerSpawn"));
            });

        Connections.Sort([](const UArrowComponent& A, const UArrowComponent& B)
            {
                return A.GetName() < B.GetName();
            });

        return Connections;
    }

    void MakeMovable(AMazeActors* Piece)
    {
        TArray<USceneComponent*> Components;
        // Move child-actor geometry with the same bounds used for placement.
        Piece->GetComponents<USceneComponent>(Components, true);

        for (USceneComponent* Component : Components)
        {
            if (IsValid(Component))
            {
                Component->SetMobility(EComponentMobility::Movable);
            }
        }
    }
}

AMazeGenerator::AMazeGenerator()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GeneratorRoot")));
}

// Called when the game starts or when spawned
void AMazeGenerator::BeginPlay()
{
    Super::BeginPlay();

    // Zero uses a random seed. A nonzero seed repeats the layout sequence.
    RandomStream.Initialize(RandomSeed == 0 ? FMath::Rand() : RandomSeed);

    // Use the first start room already placed in the level.
    CurrentRoom = InitialStartRoom;

    if (!ValidateRoom(CurrentRoom))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Configure InitialStartRoom, its three door arrows, room interface, MazeBounds, and RoomArrival trigger."));
        return;
    }

    InitializeRoom(CurrentRoom);

    // Puzzle progression opens Door1 first, then Door2 and Door3 after escapes.
    CurrentRoomEntranceTag = NAME_None;
    UnlockedDoorTags = PuzzleRoomsById.IsEmpty() ? DoorTags : TArray<FName>{ DoorTags[0] };
    LoopState = EMazeLoopState::WaitingForDoor;

    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

    if (UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this))
    {
        SaveSystem->RegisterGenerator(this);
    }
}

// Find the hallway's one EnemySpawn arrow.
UArrowComponent* AMazeGenerator::FindEnemySpawnPoint(AMazeActors* Piece) const
{
    if (!IsValid(Piece))
    {
        return nullptr;
    }

    TArray<UArrowComponent*> Arrows;
    Piece->GetComponents<UArrowComponent>(Arrows);

    UArrowComponent* SpawnPoint = nullptr;

    for (UArrowComponent* Arrow : Arrows)
    {
        if (IsValid(Arrow) && Arrow->ComponentHasTag(TEXT("EnemySpawn")))
        {
            // More than one spawn arrow means the hallway is configured incorrectly.
            if (SpawnPoint)
            {
                return nullptr;
            }

            SpawnPoint = Arrow;
        }
    }

    return SpawnPoint;
}

// Find the one enemy type assigned to this spawn arrow.
FName AMazeGenerator::FindEnemyTypeForSpawnPoint(UArrowComponent* SpawnPoint) const
{
    if (!IsValid(SpawnPoint) || !SpawnPoint->ComponentHasTag(TEXT("EnemySpawn")))
    {
        return NAME_None;
    }

    FName EnemyType = NAME_None;

    for (const TPair<FName, TSubclassOf<APawn>>& Entry : EnemyClassesByTag)
    {
        // EnemySpawn marks the position. Another tag chooses the enemy.
        if (Entry.Key.IsNone() || Entry.Key == FName(TEXT("EnemySpawn")) || !SpawnPoint->ComponentHasTag(Entry.Key))
        {
            continue;
        }

        // Each marker must match exactly one assigned enemy type.
        if (!Entry.Value || !EnemyType.IsNone())
        {
            return NAME_None;
        }

        EnemyType = Entry.Key;
    }

    return EnemyType;
}

// Use the spawn arrow's enemy type tag to choose its pawn Blueprint.
TSubclassOf<APawn> AMazeGenerator::FindEnemyClassForSpawnPoint(UArrowComponent* SpawnPoint) const
{
    const FName EnemyType = FindEnemyTypeForSpawnPoint(SpawnPoint);

    if (EnemyType.IsNone())
    {
        return nullptr;
    }

    return EnemyClassesByTag.FindRef(EnemyType);
}

// Build the requested spawn list. Unfinished types stay disabled without a count.
bool AMazeGenerator::PrepareEnemyTypes()
{
    GenerationEnemyTypes.Reset();

    if (ChaserEnemyTag.IsNone() || ChaserEnemyTag == FName(TEXT("EnemySpawn")) || ChaserEnemyTag == WeepingAngelEnemyTag || !EnemyClassesByTag.FindRef(ChaserEnemyTag))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Assign a valid ChaserEnemyTag and its pawn Blueprint. The Chaser and Weeping Angel need different tags."));
        return false;
    }

    // A positive count needs a matching class. Zero or missing counts leave that type off.
    for (const TPair<FName, int32>& Entry : EnemyHallwayCounts)
    {
        if (Entry.Key == ChaserEnemyTag || (!WeepingAngelEnemyTag.IsNone() && Entry.Key == WeepingAngelEnemyTag) || Entry.Value <= 0)
        {
            continue;
        }

        if (Entry.Key.IsNone() || Entry.Key == FName(TEXT("EnemySpawn")) || !EnemyClassesByTag.FindRef(Entry.Key))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Enemy hallway count for %s needs a matching pawn Blueprint in EnemyClassesByTag."), *Entry.Key.ToString());
            return false;
        }
    }

    for (const TPair<FName, TSubclassOf<APawn>>& Entry : EnemyClassesByTag)
    {
        // Angels belong to puzzle rooms, even if an existing Blueprint retains a hallway count.
        if (!WeepingAngelEnemyTag.IsNone() && Entry.Key == WeepingAngelEnemyTag)
        {
            continue;
        }

        const int32 Count = Entry.Key == ChaserEnemyTag ? 1 : FMath::Clamp(EnemyHallwayCounts.FindRef(Entry.Key), 0, 16);

        for (int32 Index = 0; Index < Count; ++Index)
        {
            GenerationEnemyTypes.Add(Entry.Key);
        }
    }

    const int32 HallwayCount = FMath::Clamp(MaxHallwaysForward, 1, 100);
    const int32 BranchCount = bAllowEnemyHallwaysOnBranches && bAllowJunctions ? FMath::Clamp(MaxBranchHallways, 0, 100) : 0;
    const int32 EnemySpacing = FMath::Clamp(MinHallwaysBetweenEnemyHallways, 0, 100);

    if (GenerationEnemyTypes.Num() > HallwayCount + BranchCount || (BranchCount == 0 && (GenerationEnemyTypes.Num() - 1) * (EnemySpacing + 1) >= HallwayCount))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Enemy counts or spacing do not fit %d main-route and %d possible branch hallways. Requested enemies: %d. Lower the counts/spacing or increase the hallway budgets."), HallwayCount, BranchCount, GenerationEnemyTypes.Num());
        GenerationEnemyTypes.Reset();
        return false;
    }

    // Map insertion order must not change layouts using the same random seed.
    GenerationEnemyTypes.Sort([](const FName& A, const FName& B)
        {
            return A.LexicalLess(B);
        });

    return true;
}

// Keep one shared count list. Only the first Chaser hallway has a fixed position.
bool AMazeGenerator::PrepareEnemyHallways()
{
    GenerationRemainingEnemyTypes = GenerationEnemyTypes;
    GenerationPieceLinks.Reset();
    GenerationHallwayPieces.Reset();
    GenerationEnemySpawnIndex = 0;

    if (GenerationRemainingEnemyTypes.IsEmpty() || !GenerationRemainingEnemyTypes.Contains(ChaserEnemyTag) || !IsValid(CurrentRoom))
    {
        return false;
    }

    GenerationPieceLinks.FindOrAdd(CurrentRoom.Get());
    return true;
}

// Count hallways along real connections. A nearby piece behind a wall is not a neighbour.
void AMazeGenerator::TrackMazeConnections(AMazeActors* Piece, bool bHallway)
{
    if (!IsValid(Piece))
    {
        return;
    }

    const TArray<UArrowComponent*> Connections = SortedConnections(Piece);
    TArray<TWeakObjectPtr<AMazeActors>> Neighbours;

    for (const TPair<TWeakObjectPtr<AMazeActors>, TArray<TWeakObjectPtr<AMazeActors>>>& Entry : GenerationPieceLinks)
    {
        AMazeActors* Other = Entry.Key.Get();

        if (!IsValid(Other) || Other == Piece)
        {
            continue;
        }

        for (UArrowComponent* OtherArrow : SortedConnections(Other))
        {
            // Locked, unused start-room doors are not extra routes into the maze.
            if (Other == CurrentRoom.Get() && OtherArrow != StartArrow.Get())
            {
                continue;
            }

            if (Connections.ContainsByPredicate([this, OtherArrow](UArrowComponent* Arrow)
                {
                    return ConnectionsMatch(Arrow, OtherArrow);
                }))
            {
                Neighbours.AddUnique(Other);
                break;
            }
        }
    }

    GenerationPieceLinks.FindOrAdd(Piece);

    if (bHallway)
    {
        GenerationHallwayPieces.Add(Piece);
    }

    for (const TWeakObjectPtr<AMazeActors>& Other : Neighbours)
    {
        GenerationPieceLinks.FindOrAdd(Piece).AddUnique(Other);
        GenerationPieceLinks.FindOrAdd(Other).AddUnique(Piece);
    }
}

// Rooms cost zero here, so entering a room does not reset enemy hallway spacing.
void AMazeGenerator::GetHallwayDistances(AMazeActors* From, TMap<TWeakObjectPtr<AMazeActors>, int32>& Distances) const
{
    Distances.Reset();

    if (!IsValid(From) || !GenerationPieceLinks.Contains(From))
    {
        return;
    }

    TArray<TWeakObjectPtr<AMazeActors>> PendingPieces;
    PendingPieces.Add(From);
    Distances.Add(From, 0);

    for (int32 Index = 0; Index < PendingPieces.Num(); ++Index)
    {
        const TWeakObjectPtr<AMazeActors> Piece = PendingPieces[Index];
        const TArray<TWeakObjectPtr<AMazeActors>>* Links = GenerationPieceLinks.Find(Piece);

        if (!Piece.IsValid() || !Links)
        {
            continue;
        }

        const int32 Distance = Distances.FindRef(Piece);

        for (const TWeakObjectPtr<AMazeActors>& Other : *Links)
        {
            if (!Other.IsValid())
            {
                continue;
            }

            const int32 NextDistance = Distance + (GenerationHallwayPieces.Contains(Other) ? 1 : 0);
            const int32* PreviousDistance = Distances.Find(Other);

            if (!PreviousDistance || NextDistance < *PreviousDistance)
            {
                Distances.Add(Other, NextDistance);
                PendingPieces.Add(Other);
            }
        }
    }
}

FName AMazeGenerator::ChooseEnemyHallwayType(UArrowComponent* Opening, int32 RemainingHallways)
{
    AMazeActors* Parent = IsValid(Opening) ? Cast<AMazeActors>(Opening->GetOwner()) : nullptr;

    if (!IsValid(Parent) || GenerationRemainingEnemyTypes.IsEmpty())
    {
        return NAME_None;
    }

    TMap<TWeakObjectPtr<AMazeActors>, int32> Distances;
    GetHallwayDistances(Parent, Distances);
    const int32* StartDistance = Distances.Find(CurrentRoom.Get());

    if (!StartDistance)
    {
        return NAME_None;
    }

    const int32 ParentHallway = GenerationHallwayPieces.Contains(Parent) ? 1 : 0;
    const int32 EnemySpacing = FMath::Clamp(MinHallwaysBetweenEnemyHallways, 0, 100);

    for (AMazeActors* EnemyHallway : SpawnedEnemyHallwayActors)
    {
        const int32* Distance = Distances.Find(EnemyHallway);

        if (!Distance || *Distance + ParentHallway - 1 < EnemySpacing)
        {
            return NAME_None;
        }
    }

    TArray<FName> AvailableTypes;

    for (const FName& EnemyType : GenerationRemainingEnemyTypes)
    {
        if (EnemyType != ChaserEnemyTag)
        {
            AvailableTypes.Add(EnemyType);
        }
    }

    // Share the remaining budget between the main route and branches instead of using fixed indices.
    if (AvailableTypes.IsEmpty() || RandomStream.RandRange(0, FMath::Max(1, RemainingHallways) - 1) >= GenerationRemainingEnemyTypes.Num())
    {
        return NAME_None;
    }

    return AvailableTypes[RandomStream.RandRange(0, AvailableTypes.Num() - 1)];
}

bool AMazeGenerator::RegisterEnemyHallway(AMazeActors* Piece, FName EnemyType)
{
    if (!IsValid(Piece) || GenerationRemainingEnemyTypes.RemoveSingle(EnemyType) == 0)
    {
        return false;
    }

    if (!SpawnedEnemyHallways.Contains(EnemyType))
    {
        SpawnedEnemyHallways.Add(EnemyType, Piece);
    }

    SpawnedEnemyHallwayActors.Add(Piece);
    SpawnedEnemies.Add(nullptr);
    return true;
}

// A late branch connection must not create a shorter route between enemy hallways.
bool AMazeGenerator::ValidateEnemyHallwaySpacing() const
{
    if (!GenerationRemainingEnemyTypes.IsEmpty() || SpawnedEnemyHallwayActors.Num() != GenerationEnemyTypes.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: Only %d/%d enemy hallways fit. Check enemy counts, branch capacity, and hallway spacing."), SpawnedEnemyHallwayActors.Num(), GenerationEnemyTypes.Num());
        return false;
    }

    for (AMazeActors* Hallway : SpawnedEnemyHallwayActors)
    {
        TMap<TWeakObjectPtr<AMazeActors>, int32> Distances;
        GetHallwayDistances(Hallway, Distances);
        const int32* StartDistance = Distances.Find(CurrentRoom.Get());
        const FName EnemyType = FindEnemyTypeForSpawnPoint(FindEnemySpawnPoint(Hallway));

        if (!StartDistance || EnemyType.IsNone())
        {
            return false;
        }

        for (AMazeActors* Other : SpawnedEnemyHallwayActors)
        {
            if (Other == Hallway)
            {
                continue;
            }

            const int32* Distance = Distances.Find(Other);

            if (!Distance || *Distance - 1 < FMath::Clamp(MinHallwaysBetweenEnemyHallways, 0, 100))
            {
                UE_LOG(LogTemp, Warning, TEXT("Maze: Joined branches put enemy hallways too close. Retrying the layout."));
                return false;
            }
        }
    }

    return true;
}

// Keep enemy hallway classes out of ordinary corridor selection.
bool AMazeGenerator::IsEnemyCorridorClass(TSubclassOf<AMazeActors> PieceClass) const
{
    if (!PieceClass)
    {
        return false;
    }

    for (const TSubclassOf<AMazeActors>& EnemyCorridorClass : EnemyCorridorClasses)
    {
        if (EnemyCorridorClass && PieceClass->IsChildOf(EnemyCorridorClass.Get()))
        {
            return true;
        }
    }

    return false;
}

// Rooms stay out of ordinary hallway and branch selection.
bool AMazeGenerator::IsRouteRoomClass(TSubclassOf<AMazeActors> PieceClass) const
{
    if (!PieceClass)
    {
        return false;
    }

    for (const TSubclassOf<AMazeActors>& RoomClass : RouteRoomClasses)
    {
        if (RoomClass && PieceClass->IsChildOf(RoomClass.Get()))
        {
            return true;
        }
    }

    return false;
}

bool AMazeGenerator::ShouldPlaceRouteRoom() const
{
    return !RouteRoomClasses.IsEmpty() && GenerationHallwaysSinceRoom >= FMath::Max(1, HallwaysBetweenRooms) && (MaxRouteRooms <= 0 || GenerationRouteRoomCount < MaxRouteRooms);
}

bool AMazeGenerator::ShouldPlaceBranchRoom(const FBranchRoomState& State) const
{
    return bAllowRoomsOnBranches && !RouteRoomClasses.IsEmpty() && State.HallwaysSinceRoom >= FMath::Max(1, HallwaysBetweenRooms) && (MaxBranchRooms <= 0 || GenerationBranchRoomCount < MaxBranchRooms);
}

// Spawn one enemy for this hallway instance after layout and navigation are ready.
bool AMazeGenerator::SpawnEnemyInHallway(int32 HallwayIndex, bool bSpawnImmediately)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MazeSpawnEnemy);

    if (!SpawnedEnemyHallwayActors.IsValidIndex(HallwayIndex) || !SpawnedEnemies.IsValidIndex(HallwayIndex))
    {
        return false;
    }

    // Repeated calls cannot create another enemy for an already-filled hallway.
    if (IsValid(SpawnedEnemies[HallwayIndex]))
    {
        return true;
    }

    AMazeActors* Hallway = SpawnedEnemyHallwayActors[HallwayIndex];
    UArrowComponent* EnemySpawn = FindEnemySpawnPoint(Hallway);
    const FName EnemyType = FindEnemyTypeForSpawnPoint(EnemySpawn);

    // Do not spawn a puzzle only Angel from a hallway marker.
    if (!WeepingAngelEnemyTag.IsNone() && EnemyType == WeepingAngelEnemyTag)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Weeping Angels can only spawn in puzzle rooms, not enemy hallways."));
        return false;
    }

    const TSubclassOf<APawn> EnemyToSpawn = FindEnemyClassForSpawnPoint(EnemySpawn);

    if (!GetWorld() || EnemyType.IsNone() || !EnemyToSpawn || !IsValid(EnemySpawn))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: The hallway needs one EnemySpawn arrow with exactly one enemy type assigned in EnemyClassesByTag."));
        return false;
    }

    // Only the Chaser waits. The maze can finish while its timer is pending.
    if (!bSpawnImmediately && EnemyType == ChaserEnemyTag && ChaserSpawnDelay > 0.0f)
    {
        if (!GetWorld()->GetTimerManager().IsTimerActive(ChaserSpawnTimer))
        {
            FTimerDelegate SpawnDelegate;
            SpawnDelegate.BindUObject(this, &AMazeGenerator::SpawnDelayedChaser, HallwayIndex);
            GetWorld()->GetTimerManager().SetTimer(ChaserSpawnTimer, SpawnDelegate, ChaserSpawnDelay, false);
        }

        return true;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    ++CandidateSpawnsThisFrame;

    APawn* SpawnedEnemy = GetWorld()->SpawnActor<APawn>(EnemyToSpawn, EnemySpawn->GetComponentLocation(), EnemySpawn->GetComponentRotation(), SpawnParameters);

    if (!IsValid(SpawnedEnemy))
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: Enemy %s could not spawn in %s at %s. Check EnemySpawn capsule clearance."), *EnemyType.ToString(), *Hallway->GetName(), *EnemySpawn->GetComponentLocation().ToString());
        return false;
    }

    // Use the AI Controller Class assigned to the enemy Blueprint.
    if (!SpawnedEnemy->GetController())
    {
        SpawnedEnemy->SpawnDefaultController();
    }

    if (!SpawnedEnemy->GetController())
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: The enemy Blueprint selected by the spawn tag needs an AI Controller Class."));
        SpawnedEnemy->Destroy();
        return false;
    }

    SpawnedEnemies[HallwayIndex] = SpawnedEnemy;

    // A delayed enemy needs the current room state when it first appears.
    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    APawn* Player = PlayerController ? PlayerController->GetPawn() : nullptr;

    if (EnemyType == ChaserEnemyTag && IsValid(Player) && SpawnedEnemy->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()))
    {
        IMazeInterface::Execute_PlayerRoomStateChanged(SpawnedEnemy, Player, AMazeActors::IsPlayerInRoom(Player));
    }

    return true;
}

void AMazeGenerator::SpawnDelayedChaser(int32 HallwayIndex)
{
    if (!GetWorld() || !SpawnedEnemyHallwayActors.IsValidIndex(HallwayIndex) || FindEnemyTypeForSpawnPoint(FindEnemySpawnPoint(SpawnedEnemyHallwayActors[HallwayIndex])) != ChaserEnemyTag)
    {
        return;
    }

    // Retry if something is blocking the spawn point when the timer finishes.
    if (!SpawnEnemyInHallway(HallwayIndex, true))
    {
        FTimerDelegate SpawnDelegate;
        SpawnDelegate.BindUObject(this, &AMazeGenerator::SpawnDelayedChaser, HallwayIndex);
        GetWorld()->GetTimerManager().SetTimer(ChaserSpawnTimer, SpawnDelegate, 1.0f, false);
    }
}

// Fit the navigation volume around the entire generated maze.
bool AMazeGenerator::UpdateMazeNavigationBounds()
{
    UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
    UBrushComponent* Brush = IsValid(MazeNavBounds)
        ? MazeNavBounds->GetBrushComponent()
        : nullptr;

    if (!Navigation || !IsValid(Brush) || MazeNavBounds->GetRootComponent() != Brush)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Assign a placed NavMeshBoundsVolume with its original brush root to MazeNavBounds."));
        return false;
    }

    FBox MazeWorldBounds(ForceInit);

    const auto IncludePiece = [this, &MazeWorldBounds](AMazeActors* Piece)
        {
            TArray<FBox> Boxes;

            if (!GetFootprints(Piece, Boxes))
            {
                return false;
            }

            for (const FBox& Box : Boxes)
            {
                MazeWorldBounds += Box;
            }

            return true;
        };

    // Include the current room, hallways, branches, caps, and destination rooms.
    if (!IncludePiece(CurrentRoom))
    {
        return false;
    }

    for (AMazeActors* Piece : SpawnedMazeActors)
    {
        if (IsValid(Piece) && Piece != CurrentRoom && !IncludePiece(Piece))
        {
            return false;
        }
    }

    if (!MazeWorldBounds.IsValid)
    {
        return false;
    }

    // Original brush dimensions prevent the scale from growing on later maze runs.
    const FBoxSphereBounds LocalBrushBounds = Brush->CalcBounds(FTransform::Identity);

    const FVector BaseExtent = LocalBrushBounds.BoxExtent;

    if (BaseExtent.X <= UE_SMALL_NUMBER || BaseExtent.Y <= UE_SMALL_NUMBER || BaseExtent.Z <= UE_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: MazeNavBounds needs nonzero box brush dimensions."));
        return false;
    }

    // Multiply the finished maze's width and length, then add edge clearance.
    GeneratedMazeSize = MazeWorldBounds.GetSize();

    FVector RequiredExtent = MazeWorldBounds.GetExtent();
    RequiredExtent.X *= FMath::Max(1.0f, NavBoundsWidthMultiplier);
    RequiredExtent.Y *= FMath::Max(1.0f, NavBoundsLengthMultiplier);

    RequiredExtent += FVector(
        FMath::Max(0.0f, NavBoundsHorizontalPadding),
        FMath::Max(0.0f, NavBoundsHorizontalPadding),
        FMath::Max(0.0f, NavBoundsHeightPadding));

    const FVector NewScale(
        RequiredExtent.X / BaseExtent.X,
        RequiredExtent.Y / BaseExtent.Y,
        RequiredExtent.Z / BaseExtent.Z);

    // Center the volume even when its original brush pivot is offset.
    const FVector NewLocation = MazeWorldBounds.GetCenter() - LocalBrushBounds.Origin * NewScale;

    Brush->SetMobility(EComponentMobility::Movable);

    MazeNavBounds->SetActorTransform(FTransform(FQuat::Identity, NewLocation, NewScale));

    Brush->UpdateBounds();

    // Notify navigation after moving and resizing the volume.
    GeneratedNavBoundsSize = RequiredExtent * 2.0;
    Navigation->OnNavigationBoundsUpdated(MazeNavBounds);

    return true;
}

// Wait for navigation for every enemy type before spawning any enemies.
void AMazeGenerator::StepNavigation()
{
    UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());

    if (!Navigation || GenerationEnemyTypes.IsEmpty() || SpawnedEnemyHallwayActors.Num() != GenerationEnemyTypes.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Navigation or the required enemy hallways became invalid."));
        BeginGenerationCleanup(false);
        return;
    }

    if (FPlatformTime::Seconds() - NavigationWaitStartedAt >= FMath::Max(1.0f, NavigationBuildTimeout))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Navigation timed out. Check Dynamic generation, supported agent size, floor collision, and EnemySpawn placement."));
        BeginGenerationCleanup(false);
        return;
    }

    // Bounds updates and dirty tiles must finish before the enemy starts moving.
    if (!Navigation->PendingNavBoundsUpdates.IsEmpty() || Navigation->HasDirtyAreasQueued() || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(this))
    {
        return;
    }

    for (AMazeActors* Hallway : SpawnedEnemyHallwayActors)
    {
        UArrowComponent* EnemySpawn = FindEnemySpawnPoint(Hallway);
        const FName EnemyType = FindEnemyTypeForSpawnPoint(EnemySpawn);
        const TSubclassOf<APawn> EnemyToSpawn = FindEnemyClassForSpawnPoint(EnemySpawn);
        const APawn* EnemyDefaults = EnemyToSpawn.GetDefaultObject();

        if (!IsValid(Hallway) || !IsValid(EnemySpawn) || !EnemyDefaults || EnemyType.IsNone() || !GenerationEnemyTypes.Contains(EnemyType))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: The hallway for enemy type %s is configured incorrectly."), *EnemyType.ToString());
            BeginGenerationCleanup(false);
            return;
        }

        ANavigationData* NavData = Navigation->GetNavDataForProps(EnemyDefaults->GetNavAgentPropertiesRef());

        if (!NavData)
        {
            return;
        }

        if (NavData->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic)
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Set Runtime Generation to Dynamic for enemy type %s."), *EnemyType.ToString());
            BeginGenerationCleanup(false);
            return;
        }

        // Each enemy needs a walkable surface below its own spawn marker.
        FNavLocation NavigableSpawn;

        if (!Navigation->ProjectPointToNavigation(EnemySpawn->GetComponentLocation(), NavigableSpawn, FVector(50.0, 50.0, 500.0), NavData))
        {
            return;
        }
    }

    GenerationEnemySpawnIndex = 0;
    GenerationStage = EGenerationStage::Enemies;
}

void AMazeGenerator::ClearGeneratedLayout()
{
    CancelGeneration();

    // A cleared or rebuilt hallway must not spawn an old Chaser later.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChaserSpawnTimer);
    }

    ClearPendingArrival();

    OpenConnections.Empty();
    StartArrow.Reset();
    EndArrow.Reset();

    for (APawn* Enemy : SpawnedEnemies)
    {
        if (IsValid(Enemy))
        {
            AController* Controller = Enemy->GetController();

            if (IsValid(Controller) && !Controller->IsPlayerController())
            {
                Controller->Destroy();
            }

            Enemy->Destroy();
        }
    }

    SpawnedEnemies.Reset();
    SpawnedEnemyHallways.Reset();
    SpawnedEnemyHallwayActors.Reset();
    GeneratedMazeSize = FVector::ZeroVector;
    GeneratedNavBoundsSize = FVector::ZeroVector;

    // Remove the generated layout while keeping the current start room.
    for (AMazeActors* Piece : SpawnedMazeActors)
    {
        if (IsValid(Piece) && Piece != CurrentRoom)
        {
            Piece->Destroy();
        }
    }

    SpawnedMazeActors.Empty();

    SpawnedEndRoom = nullptr;
    SpawnedPuzzleRoom = nullptr;
    bPuzzlePrepared = false;
    NextRoom = nullptr;
    NextRoomEntranceTag = NAME_None;

    bMainRouteComplete = false;
    UnfinishedBranchOpenings = 0;
    StartConnectionTransform = FTransform::Identity;
    EndConnectionTransform = FTransform::Identity;
}

bool AMazeGenerator::GetFootprints(AMazeActors* Piece, TArray<FBox>& Boxes) const
{
    Boxes.Reset();

    if (!IsValid(Piece))
    {
        return false;
    }

    TArray<UBoxComponent*> Components;
    // A room may keep extra bounds inside child actors.
    Piece->GetComponents<UBoxComponent>(Components, true);

    for (UBoxComponent* Box : Components)
    {
        if (!IsValid(Box) || !Box->ComponentHasTag(FName(TEXT("MazeBounds"))))
        {
            continue;
        }

        // Convert the local box into a world-space footprint.
        const FVector Extent = Box->GetUnscaledBoxExtent();

        if (Extent.ContainsNaN() || Box->GetComponentTransform().ContainsNaN() || Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
        {
            // Temporary: identify bounds that cannot be used for placement.
            UE_LOG(LogTemp, Warning, TEXT("Maze: %s has invalid MazeBounds %s. Extent: %s."), *Piece->GetName(), *Box->GetName(), *Extent.ToString());
            return false;
        }

        const FBox WorldBox = FBox(-Extent, Extent).TransformBy(Box->GetComponentTransform());
        const FVector Size = WorldBox.GetSize();

        // Zero scale must not turn an occupied room into an empty footprint.
        if (!WorldBox.IsValid || WorldBox.Min.ContainsNaN() || WorldBox.Max.ContainsNaN() || Size.X <= 0.0 || Size.Y <= 0.0 || Size.Z <= 0.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Maze: %s has collapsed or invalid world bounds on %s."), *Piece->GetName(), *Box->GetName());
            return false;
        }

        Boxes.Add(WorldBox);
    }

    if (Boxes.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: %s has no box components tagged MazeBounds."), *Piece->GetName());
    }

    return !Boxes.IsEmpty();
}

bool AMazeGenerator::IsBoundsSeamAllowed(AMazeActors* First, AMazeActors* Second, const FBox& Overlap) const
{
    const double Tolerance = FMath::Max(0.0f, BoundsSeamTolerance);

    if (Tolerance <= 0.0 || !IsValid(First) || !IsValid(Second))
    {
        return false;
    }

    const TArray<UArrowComponent*> FirstConnections = SortedConnections(First);
    const TArray<UArrowComponent*> SecondConnections = SortedConnections(Second);

    for (UArrowComponent* A : FirstConnections)
    {
        for (UArrowComponent* B : SecondConnections)
        {
            if (!ConnectionsMatch(A, B))
            {
                continue;
            }

            FVector Forward = A->GetForwardVector();
            Forward.Z = 0.0;
            Forward = Forward.GetSafeNormal();

            if (Forward.IsNearlyZero())
            {
                continue;
            }

            // Only a thin overlap at the shared doorway plane is a valid seam.
            const double Distance = FMath::Abs(FVector::DotProduct(Overlap.GetCenter() - A->GetComponentLocation(), Forward));
            const double Depth = FVector::DotProduct(Overlap.GetExtent(), Forward.GetAbs());

            if (Distance + Depth <= Tolerance + KINDA_SMALL_NUMBER)
            {
                return true;
            }
        }
    }

    return false;
}

bool AMazeGenerator::FootprintsOverlap(const TArray<FBox>& A, const TArray<FBox>& B) const
{
    const double Tolerance = FMath::Max(0.0f, BoundsSeamTolerance);

    for (const FBox& First : A)
    {
        for (const FBox& Second : B)
        {
            const double OverlapX = FMath::Min(First.Max.X, Second.Max.X) - FMath::Max(First.Min.X, Second.Min.X);
            const double OverlapY = FMath::Min(First.Max.Y, Second.Max.Y) - FMath::Max(First.Min.Y, Second.Min.Y);

            if (OverlapX > Tolerance && OverlapY > Tolerance)
            {
                return true;
            }
        }
    }

    return false;
}

bool AMazeGenerator::PrepareEndCapFootprint()
{
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ++CandidateSpawnsThisFrame;
    AMazeActors* Cap = GetWorld()->SpawnActor<AMazeActors>(EndCapClass, FVector::ZeroVector, FRotator::ZeroRotator, Parameters);

    if (!IsValid(Cap))
    {
        return false;
    }

    MakeMovable(Cap);
    const TArray<UArrowComponent*> Connections = SortedConnections(Cap);
    TArray<FBox> Boxes;

    if (Connections.Num() != 1 || !GetFootprints(Cap, Boxes))
    {
        Cap->Destroy();
        return false;
    }

    const FTransform ActorTransform = Cap->GetActorTransform();
    EndCapEntranceLocation = ActorTransform.InverseTransformPosition(Connections[0]->GetComponentLocation());
    EndCapEntranceForward = ActorTransform.InverseTransformVectorNoScale(Connections[0]->GetForwardVector());
    EndCapScale = Cap->GetActorScale3D();
    EndCapFootprintCorners.Reset();

    TArray<UBoxComponent*> Components;
    // Reserve the same child-actor bounds that the real cap will use.
    Cap->GetComponents<UBoxComponent>(Components, true);

    for (UBoxComponent* Box : Components)
    {
        if (!IsValid(Box) || !Box->ComponentHasTag(TEXT("MazeBounds")))
        {
            continue;
        }

        const FVector Extent = Box->GetUnscaledBoxExtent();

        // Keep eight corners per box so rotated and scaled bounds stay accurate.
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector LocalCorner(
                (Corner & 1) ? Extent.X : -Extent.X,
                (Corner & 2) ? Extent.Y : -Extent.Y,
                (Corner & 4) ? Extent.Z : -Extent.Z);

            EndCapFootprintCorners.Add(ActorTransform.InverseTransformPosition(Box->GetComponentTransform().TransformPosition(LocalCorner)));
        }
    }

    Cap->Destroy();
    return !EndCapFootprintCorners.IsEmpty();
}

bool AMazeGenerator::GetEndCapFootprints(UArrowComponent* Connection, TArray<FBox>& Boxes) const
{
    Boxes.Reset();

    if (!IsValid(Connection) || EndCapFootprintCorners.IsEmpty())
    {
        return false;
    }

    // Match the real cap's entrance to this opening without spawning another actor.
    const float Yaw = FMath::UnwindDegrees(Connection->GetForwardVector().Rotation().Yaw + 180.0f - EndCapEntranceForward.Rotation().Yaw);
    FTransform Transform(FRotator(0.0f, Yaw, 0.0f), FVector::ZeroVector, EndCapScale);
    Transform.SetLocation(Connection->GetComponentLocation() - Transform.TransformPosition(EndCapEntranceLocation));

    for (int32 Index = 0; Index < EndCapFootprintCorners.Num(); Index += 8)
    {
        FBox Box(ForceInit);

        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            Box += Transform.TransformPosition(EndCapFootprintCorners[Index + Corner]);
        }

        Boxes.Add(Box);
    }

    return true;
}

void AMazeGenerator::GetReservedCapFootprints(TArray<FBox>& Boxes, UArrowComponent* IgnoredOpening) const
{
    Boxes.Reset();
    TArray<TWeakObjectPtr<UArrowComponent>> Openings = OpenConnections;
    Openings.Append(BlockedConnections);

    for (const TWeakObjectPtr<UArrowComponent>& Opening : Openings)
    {
        if (Opening.Get() != IgnoredOpening)
        {
            TArray<FBox> CapBoxes;

            if (GetEndCapFootprints(Opening.Get(), CapBoxes))
            {
                Boxes.Append(CapBoxes);
            }
        }
    }
}

bool AMazeGenerator::CanReserveEndCaps(AMazeActors* Piece, UArrowComponent* Entrance, UArrowComponent* ReservedExit) const
{
    TArray<FBox> ReservedBoxes;
    GetReservedCapFootprints(ReservedBoxes);
    TArray<FBox> PieceBoxes;

    if (!GetFootprints(Piece, PieceBoxes))
    {
        return false;
    }

    for (UArrowComponent* Connection : SortedConnections(Piece))
    {
        if (Connection == Entrance || Connection == ReservedExit)
        {
            continue;
        }

        TArray<FBox> CapBoxes;

        if (!GetEndCapFootprints(Connection, CapBoxes) || FootprintsOverlap(CapBoxes, PieceBoxes) || FootprintsOverlap(CapBoxes, ReservedBoxes))
        {
            return false;
        }

        const auto HitsExisting = [this, Piece, &CapBoxes](AMazeActors* Existing)
            {
                if (!IsValid(Existing) || Existing == Piece)
                {
                    return false;
                }

                TArray<FBox> ExistingBoxes;
                return !GetFootprints(Existing, ExistingBoxes) || FootprintsOverlap(CapBoxes, ExistingBoxes);
            };

        if (HitsExisting(CurrentRoom) || SpawnedMazeActors.ContainsByPredicate(HitsExisting))
        {
            return false;
        }

        // Each new side opening also reserves space against the other new caps.
        ReservedBoxes.Append(CapBoxes);
    }

    return true;
}

bool AMazeGenerator::IsConnectionCrowded(UArrowComponent* Connection) const
{
    if (!IsValid(Connection))
    {
        return true;
    }

    const double Radius = FMath::Max(0.0f, ConnectionClearanceRadius);

    if (Radius <= 0.0)
    {
        return false;
    }

    // Check in front of the arrow. The piece behind its own opening is expected.
    FVector Forward = Connection->GetForwardVector();
    Forward.Z = 0.0;
    Forward = Forward.GetSafeNormal();
    const FVector Probe = Connection->GetComponentLocation() + Forward * (Radius + FMath::Max(0.0f, BoundsSeamTolerance));

    const auto IsNearPiece = [this, Connection, Probe, Radius](AMazeActors* Existing)
        {
            if (!IsValid(Existing) || Existing == Connection->GetOwner())
            {
                return false;
            }

            TArray<FBox> Boxes;

            if (!GetFootprints(Existing, Boxes))
            {
                return true;
            }

            for (const FBox& Box : Boxes)
            {
                const double DX = Probe.X - FMath::Clamp(Probe.X, Box.Min.X, Box.Max.X);
                const double DY = Probe.Y - FMath::Clamp(Probe.Y, Box.Min.Y, Box.Max.Y);

                if (DX * DX + DY * DY < Radius * Radius)
                {
                    return true;
                }
            }

            return false;
        };

    return IsNearPiece(CurrentRoom) || SpawnedMazeActors.ContainsByPredicate(IsNearPiece);
}

TArray<UArrowComponent*> AMazeGenerator::GetRouteExits(AMazeActors* Piece, UArrowComponent* Entrance) const
{
    TArray<UArrowComponent*> Exits = SortedConnections(Piece);
    Exits.Remove(Entrance);

    Exits.RemoveAll([this, Piece, Entrance](UArrowComponent* Exit)
        {
            for (const TWeakObjectPtr<UArrowComponent>& Open : OpenConnections)
            {
                if (ConnectionsMatch(Exit, Open.Get()))
                {
                    return true;
                }
            }

            return IsConnectionCrowded(Exit) || !CanReserveEndCaps(Piece, Entrance, Exit);
        });

    return Exits;
}

bool AMazeGenerator::OverlapsMaze(AMazeActors* Piece, UArrowComponent* IgnoredOpening) const
{
    TArray<FBox> CandidateBoxes;

    if (!GetFootprints(Piece, CandidateBoxes))
    {
        return true;
    }

    const double Tolerance = FMath::Max(0.0f, BoundsSeamTolerance);

    const auto OverlapsExisting = [this, Piece, &CandidateBoxes, Tolerance](AMazeActors* Existing)
        {
            if (!IsValid(Existing) || Existing == Piece)
            {
                return false;
            }

            TArray<FBox> ExistingBoxes;

            if (!GetFootprints(Existing, ExistingBoxes))
            {
                return true;
            }

            for (const FBox& A : CandidateBoxes)
            {
                for (const FBox& B : ExistingBoxes)
                {
                    const double OverlapX = FMath::Min(A.Max.X, B.Max.X) - FMath::Max(A.Min.X, B.Min.X);
                    const double OverlapY = FMath::Min(A.Max.Y, B.Max.Y) - FMath::Max(A.Min.Y, B.Min.Y);

                    if (OverlapX > KINDA_SMALL_NUMBER && OverlapY > KINDA_SMALL_NUMBER)
                    {
                        const FBox Overlap(
                            FVector(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y), 0.0),
                            FVector(FMath::Min(A.Max.X, B.Max.X), FMath::Min(A.Max.Y, B.Max.Y), 0.0));

                        // Unrelated pieces cannot use doorway seam tolerance to enter a room.
                        if (IsBoundsSeamAllowed(Piece, Existing, Overlap))
                        {
                            continue;
                        }

                        UE_LOG(LogTemp, Warning, TEXT("Maze: %s overlaps %s outside a connected doorway seam. OverlapX=%.2f, OverlapY=%.2f, SeamTolerance=%.2f."), *Piece->GetName(), *Existing->GetName(), OverlapX, OverlapY, Tolerance);
                        return true;
                    }
                }
            }

            return false;
        };

    // The active room participates in bounds checks but is not owned by the generated layout.
    if (OverlapsExisting(CurrentRoom))
    {
        return true;
    }

    for (AMazeActors* Existing : SpawnedMazeActors)
    {
        if (OverlapsExisting(Existing))
        {
            return true;
        }
    }

    // Later pieces must leave room to close every unused side opening.
    TArray<FBox> ReservedBoxes;
    GetReservedCapFootprints(ReservedBoxes, IgnoredOpening);

    if (FootprintsOverlap(CandidateBoxes, ReservedBoxes))
    {
        return true;
    }

    return false;
}

bool AMazeGenerator::ConnectionsMatch(UArrowComponent* A, UArrowComponent* B) const
{
    if (!IsValid(A) || !IsValid(B) || A->GetOwner() == B->GetOwner())
    {
        return false;
    }

    return FVector::Dist(A->GetComponentLocation(), B->GetComponentLocation()) <= FMath::Max(0.1f, ConnectionTolerance)
        && FVector::DotProduct(A->GetForwardVector(), B->GetForwardVector()) < -0.999f;
}

UArrowComponent* AMazeGenerator::FindTaggedConnection(AMazeActors* MazeActor, const FName& RequiredTag) const
{
    if (!IsValid(MazeActor) || RequiredTag.IsNone())
    {
        return nullptr;
    }

    for (UArrowComponent* Connection : SortedConnections(MazeActor))
    {
        if (Connection->ComponentHasTag(RequiredTag))
        {
            return Connection;
        }
    }

    return nullptr;
}

void AMazeGenerator::StepMainRoute()
{
    UArrowComponent* PreviousExit = GenerationPreviousExit.Get();

    if (!IsValid(PreviousExit))
    {
        BeginGenerationCleanup(true);
        return;
    }

    // Rooms are extra pieces and do not consume an enemy hallway position.
    const bool bRouteRoom = ShouldPlaceRouteRoom();
    const int32 BranchBudget = bAllowEnemyHallwaysOnBranches && bAllowJunctions ? GenerationBranchLimit : 0;
    const FName EnemyType = bAttachmentPending ? PendingEnemyType : bRouteRoom ? NAME_None : GenerationHallwayIndex == 0 ? ChaserEnemyTag : ChooseEnemyHallwayType(PreviousExit, GenerationHallwayCount - GenerationHallwayIndex + BranchBudget);
    const bool bEnemyHallwaySlot = !EnemyType.IsNone();

    if (!bAttachmentPending)
    {
        bTryingMainJunction = !bRouteRoom && !bEnemyHallwaySlot && bAllowJunctions && GenerationHallwayIndex > 0 && GenerationHallwayIndex < GenerationHallwayCount - 1 && GenerationHallwayIndex >= GenerationNextJunctionAt;

        StartAttachment(PreviousExit->GetComponentLocation(), PreviousExit->GetForwardVector(), bTryingMainJunction ? 3 : 2, bTryingMainJunction ? 4 : 2, false, EnemyType, bRouteRoom);
    }

    UArrowComponent* Entrance = nullptr;
    AMazeActors* Piece = nullptr;
    const EAttachmentResult Result = StepAttachment(Piece, Entrance);

    if (Result == EAttachmentResult::Pending)
    {
        return;
    }

    if (Result == EAttachmentResult::Failed)
    {
        if (bEnemyHallwaySlot && EnemyType != ChaserEnemyTag)
        {
            // Keep this enemy available for a later hallway if its piece cannot fit here.
            bTryingMainJunction = bAllowJunctions && GenerationHallwayIndex < GenerationHallwayCount - 1 && GenerationHallwayIndex >= GenerationNextJunctionAt;
            StartAttachment(PreviousExit->GetComponentLocation(), PreviousExit->GetForwardVector(), bTryingMainJunction ? 3 : 2, bTryingMainJunction ? 4 : 2, false);
            return;
        }

        if (bTryingMainJunction)
        {
            // A junction that cannot fit falls back to a straight or corner piece.
            bTryingMainJunction = false;
            StartAttachment(PreviousExit->GetComponentLocation(), PreviousExit->GetForwardVector(), 2, 2, false);
            return;
        }

        BeginGenerationCleanup(true);
        return;
    }

    TArray<UArrowComponent*> Exits = GetRouteExits(Piece, Entrance);

    if (Exits.IsEmpty())
    {
        Piece->Destroy();
        BeginGenerationCleanup(true);
        return;
    }

    // The only Chaser hallway still connects directly to the start room.
    if (bEnemyHallwaySlot && EnemyType == ChaserEnemyTag && (GenerationHallwayIndex != 0 || SpawnedEnemyHallways.Contains(EnemyType)))
    {
        Piece->Destroy();
        BeginGenerationCleanup(true);
        return;
    }

    SpawnedMazeActors.Add(Piece);
    TrackMazeConnections(Piece, !bRouteRoom);

    // Consume the shared count only after this enemy hallway successfully attaches.
    if (bEnemyHallwaySlot && !RegisterEnemyHallway(Piece, EnemyType))
    {
        BeginGenerationCleanup(true);
        return;
    }

    UArrowComponent* Exit = Exits[RandomStream.RandRange(0, Exits.Num() - 1)];

    // At junctions, prefer a clear exit that spreads away from the start.
    if (ConnectionClearanceRadius > 0.0f && StartArrow.IsValid())
    {
        for (UArrowComponent* CandidateExit : Exits)
        {
            if (FVector::DistSquared2D(CandidateExit->GetComponentLocation(), StartArrow->GetComponentLocation()) > FVector::DistSquared2D(Exit->GetComponentLocation(), StartArrow->GetComponentLocation()))
            {
                Exit = CandidateExit;
            }
        }
    }

    if (SortedConnections(Piece).Num() > 2)
    {
        GenerationNextJunctionAt = GenerationHallwayIndex + RandomStream.RandRange(GenerationMinJunctionSpacing, GenerationMaxJunctionSpacing);
    }

    AddUnusedConnections(Piece, Entrance, Exit);
    JoinMatchingOpenConnections();

    GenerationPreviousExit = Exit;

    // Track rooms separately from hallway pieces.
    if (bRouteRoom)
    {
        // Count only rooms that were successfully attached.
        GenerationConsecutiveRouteRooms = Piece->GetClass() == GenerationLastRouteRoomClass.Get() ? GenerationConsecutiveRouteRooms + 1 : 1;
        GenerationLastRouteRoomClass = Piece->GetClass();
        ++GenerationRouteRoomCount;
        GenerationHallwaysSinceRoom = 0;
    }
    else
    {
        ++GenerationHallwayIndex;
        ++GenerationHallwaysSinceRoom;
    }

    // Insert the puzzle once, then continue building the remaining main route.
    if (ActivePuzzleRoomClass && !IsValid(SpawnedPuzzleRoom) && GenerationHallwayIndex >= GenerationPuzzleHallwayIndex)
    {
        EndArrow = Exit;
        EndConnectionTransform = Exit->GetComponentTransform();
        GenerationStage = EGenerationStage::PuzzleRoom;
    }
    // Place a final scheduled room before the extra exit hallways.
    else if (GenerationHallwayIndex == GenerationHallwayCount && !ShouldPlaceRouteRoom())
    {
        EndArrow = Exit;
        EndConnectionTransform = Exit->GetComponentTransform();
        GenerationStage = PuzzleRoomsById.IsEmpty() ? EGenerationStage::EndRoom : EGenerationStage::ExitHallways;
    }
}

TArray<FName> AMazeGenerator::GetRemainingPuzzleIds() const
{
    TArray<FName> Remaining;

    for (const auto& Entry : PuzzleRoomsById)
    {
        if (!Entry.Key.IsNone() && Entry.Value && !CompletedPuzzleIds.Contains(Entry.Key))
        {
            Remaining.Add(Entry.Key);
        }
    }

    // Map iteration order must not change seeded puzzle selection.
    Remaining.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Remaining;
}

bool AMazeGenerator::SelectPuzzleRoom()
{
    if (PuzzleRoomsById.IsEmpty())
    {
        ActivePuzzleId = NAME_None;
        ActivePuzzleRoomClass = nullptr;
        bCurrentPuzzleCompleted = false;
        return true;
    }

    if (PuzzleRoomsById.Num() != 3 || PuzzleRoomExitTag.IsNone() || MazeEndDoorTag.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Assign exactly three different PuzzleRoomsById entries and valid puzzle/end-door tags."));
        return false;
    }

    TSet<UClass*> RoomClasses;

    for (const auto& Entry : PuzzleRoomsById)
    {
        if (Entry.Key.IsNone() || !Entry.Value || RoomClasses.Contains(Entry.Value.Get()) || IsEnemyCorridorClass(Entry.Value) || IsRouteRoomClass(Entry.Value) || CorridorClasses.Contains(Entry.Value))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Puzzle rooms need different IDs/classes and must not also appear in corridor, card, trap, or enemy hallway arrays."));
            return false;
        }

        RoomClasses.Add(Entry.Value.Get());
    }

    // A debug rebuild keeps the selected puzzle and rolls back its completion.
    if (bRebuildingPuzzle)
    {
        const TSubclassOf<AMazeActors>* RoomClass = PuzzleRoomsById.Find(ActivePuzzleId);
        return RoomClass && *RoomClass == ActivePuzzleRoomClass;
    }

    const TArray<FName> Remaining = GetRemainingPuzzleIds();

    if (Remaining.IsEmpty())
    {
        return false;
    }

    ActivePuzzleId = Remaining[RandomStream.RandRange(0, Remaining.Num() - 1)];
    ActivePuzzleRoomClass = PuzzleRoomsById.FindChecked(ActivePuzzleId);
    bCurrentPuzzleCompleted = false;
    return true;
}

UMazePuzzleComponent* AMazeGenerator::GetPuzzleComponent() const
{
    return IsValid(SpawnedPuzzleRoom) ? SpawnedPuzzleRoom->FindComponentByClass<UMazePuzzleComponent>() : nullptr;
}

bool AMazeGenerator::RegisterPuzzleCompleted(AMazeActors* Room)
{
    UMazePuzzleComponent* Puzzle = GetPuzzleComponent();

    if (LoopState != EMazeLoopState::TraversingMaze || !IsValid(Room) || Room != SpawnedPuzzleRoom || !IsValid(Puzzle) || !Puzzle->bButtonsUnlocked || !Puzzle->bPuzzleDoorUnlocked || (Puzzle->bUsePressureButtons && !Puzzle->bPuzzleStarted) || bCurrentPuzzleCompleted || ActivePuzzleId.IsNone() || CompletedPuzzleIds.Contains(ActivePuzzleId) || !IsValid(SpawnedEndRoom))
    {
        return false;
    }

    // Selecting or spawning a room never counts as completing its puzzle.
    CompletedPuzzleIds.Add(ActivePuzzleId);
    bCurrentPuzzleCompleted = true;
    IMazeInterface::Execute_SetMazeDoorLocked(SpawnedEndRoom, MazeEndDoorTag, false);
    OnPuzzleCompleted.Broadcast(ActivePuzzleId);
    return true;
}

bool AMazeGenerator::SpawnEndRoom()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MazeSpawnEndRoom);

    if (!EndRoomClass || !EndArrow.IsValid())
    {
        return false;
    }

    const FVector TargetPosition = EndArrow->GetComponentLocation();
    const FVector TargetForward = EndArrow->GetForwardVector();

    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMazeActors* Candidate = GetWorld()->SpawnActor<AMazeActors>(EndRoomClass, TargetPosition, FRotator::ZeroRotator, Parameters);

    if (!IsValid(Candidate))
    {
        return false;
    }

    MakeMovable(Candidate);

    TArray<FBox> Footprints;
    UArrowComponent* RoomExit = FindTaggedConnection(Candidate, EndRoomExitTag);

    if (!IsValid(RoomExit) || !GetFootprints(Candidate, Footprints))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: EndRoomClass needs MazeBounds and an exit arrow tagged %s."), *EndRoomExitTag.ToString());

        Candidate->Destroy();
        return false;
    }

    // Reserve the tagged exit for the next start room.
    TArray<UArrowComponent*> Entrances = SortedConnections(Candidate);
    Entrances.Remove(RoomExit);

    Entrances.RemoveAll([this](UArrowComponent* Arrow)
        {
            return Arrow->ComponentHasTag(EndRoomExitTag);
        });

    const bool bHasDesignatedEntrance = Entrances.ContainsByPredicate([](UArrowComponent* Arrow)
        {
            return Arrow->ComponentHasTag(TEXT("MazeEntrance"));
        });

    if (bHasDesignatedEntrance)
    {
        Entrances.RemoveAll([](UArrowComponent* Arrow)
            {
                return !Arrow->ComponentHasTag(TEXT("MazeEntrance"));
            });
    }

    for (int32 Index = Entrances.Num() - 1; Index > 0; --Index)
    {
        Entrances.Swap(Index, RandomStream.RandRange(0, Index));
    }

    for (UArrowComponent* Entrance : Entrances)
    {
        Candidate->SetActorLocationAndRotation(TargetPosition, FRotator::ZeroRotator);

        const float Yaw = FMath::UnwindDegrees(TargetForward.Rotation().Yaw + 180.0f - Entrance->GetForwardVector().Rotation().Yaw);

        Candidate->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
        Candidate->SetActorLocation(Candidate->GetActorLocation() + TargetPosition - Entrance->GetComponentLocation());

        const bool bFacingCorrectly = FVector::DotProduct(Entrance->GetForwardVector(), TargetForward.GetSafeNormal()) <= -0.999f;
        const bool bPositionMatches = FVector::Dist(Entrance->GetComponentLocation(), TargetPosition) <= FMath::Max(0.1f, ConnectionTolerance);

        if (bFacingCorrectly && bPositionMatches && !OverlapsMaze(Candidate))
        {
            SpawnedEndRoom = Candidate;
            SpawnedMazeActors.Add(Candidate);
            TrackMazeConnections(Candidate, false);

            return true;
        }
    }

    Candidate->Destroy();

    return false;
}

void AMazeGenerator::StartAttachment(const FVector& TargetPosition, const FVector& TargetForward, int32 MinConnections, int32 MaxConnections, bool bCap, FName EnemyType, bool bRouteRoom, TSubclassOf<AMazeActors> ForcedClass)
{
    PendingTargetPosition = TargetPosition;
    PendingTargetForward = TargetForward;
    PendingMinConnections = MinConnections;
    PendingMaxConnections = MaxConnections;
    PendingAttachmentClassIndex = 0;
    PendingAttachmentClasses.Reset();
    bAttachmentPending = true;
    PendingEnemyType = bCap ? NAME_None : EnemyType;

    if (ForcedClass)
    {
        PendingAttachmentClasses.Add(ForcedClass);
    }
    else if (bCap)
    {
        if (EndCapClass)
        {
            PendingAttachmentClasses.Add(EndCapClass);
        }
    }
    else if (bRouteRoom)
    {
        // Side paths use the room history inherited from their own junction.
        const bool bBranchRoom = GenerationStage == EGenerationStage::Branches;
        const UClass* PreviousRoomClass = bBranchRoom ? GenerationBranchRoomState.LastRoomClass.Get() : GenerationLastRouteRoomClass.Get();
        const int32 PreviousRoomCount = bBranchRoom ? GenerationBranchRoomState.ConsecutiveRooms : GenerationConsecutiveRouteRooms;
        const int32 RoomNumber = bBranchRoom ? GenerationBranchRoomCount : GenerationRouteRoomCount;
        int32 RoomIndex = bRandomizeRouteRooms ? RandomStream.RandRange(0, RouteRoomClasses.Num() - 1) : RoomNumber % RouteRoomClasses.Num();

        // After two matching rooms, the next room must use another type.
        if (PreviousRoomCount >= 2 && RouteRoomClasses[RoomIndex].Get() == PreviousRoomClass)
        {
            TArray<int32> OtherRoomIndices;

            for (int32 Index = 0; Index < RouteRoomClasses.Num(); ++Index)
            {
                if (RouteRoomClasses[Index] && RouteRoomClasses[Index].Get() != PreviousRoomClass)
                {
                    OtherRoomIndices.Add(Index);
                }
            }

            // A maze with only one room type can still generate.
            if (!OtherRoomIndices.IsEmpty())
            {
                RoomIndex = OtherRoomIndices[bRandomizeRouteRooms ? RandomStream.RandRange(0, OtherRoomIndices.Num() - 1) : 0];
            }
        }

        PendingAttachmentClasses.Add(RouteRoomClasses[RoomIndex]);
    }
    else if (!PendingEnemyType.IsNone())
    {
        PendingAttachmentClasses = EnemyCorridorClasses;
    }
    else
    {
        PendingAttachmentClasses = CorridorClasses;
        PendingAttachmentClasses.RemoveAll([this](const TSubclassOf<AMazeActors>& PieceClass)
            {
                return !PieceClass || IsEnemyCorridorClass(PieceClass) || IsRouteRoomClass(PieceClass);
            });
    }

    // Shuffle once. Continuing next frame does not change the random order.
    for (int32 Index = PendingAttachmentClasses.Num() - 1; Index > 0; --Index)
    {
        PendingAttachmentClasses.Swap(Index, RandomStream.RandRange(0, Index));
    }
}

AMazeGenerator::EAttachmentResult AMazeGenerator::StepAttachment(AMazeActors*& Piece, UArrowComponent*& Entrance)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MazeTryAttachCandidate);

    Piece = nullptr;
    Entrance = nullptr;

    const auto ContinueOrFail = [this]()
        {
            if (PendingAttachmentClassIndex < PendingAttachmentClasses.Num())
            {
                return EAttachmentResult::Pending;
            }

            UE_LOG(LogTemp, Warning, TEXT("Maze: No candidate fits at %s. Required connections: %d-%d. Enemy type: %s."), *PendingTargetPosition.ToString(), PendingMinConnections, PendingMaxConnections, *PendingEnemyType.ToString());

            bAttachmentPending = false;
            PendingAttachmentClasses.Reset();

            return EAttachmentResult::Failed;
        };

    if (PendingAttachmentClassIndex >= PendingAttachmentClasses.Num())
    {
        return ContinueOrFail();
    }

    const TSubclassOf<AMazeActors> PieceClass = PendingAttachmentClasses[PendingAttachmentClassIndex++];

    if (!PieceClass)
    {
        return ContinueOrFail();
    }

    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Only this candidate is tested. Further classes wait for another step.
    // Keep gameplay events out of candidate corridor BeginPlay.
    ++CandidateSpawnsThisFrame;

    AMazeActors* Candidate = GetWorld()->SpawnActor<AMazeActors>(PieceClass, PendingTargetPosition, FRotator::ZeroRotator, Parameters);

    if (!IsValid(Candidate))
    {
        return ContinueOrFail();
    }

    MakeMovable(Candidate);

    TArray<UArrowComponent*> SpawnArrows;
    Candidate->GetComponents<UArrowComponent>(SpawnArrows);
    const int32 EnemySpawnCount = SpawnArrows.FilterByPredicate([](UArrowComponent* Arrow)
        {
            return IsValid(Arrow) && Arrow->ComponentHasTag(TEXT("EnemySpawn"));
        }).Num();

    const bool bEnemyHallway = !PendingEnemyType.IsNone();

    // Only a chosen enemy placement may use an enemy hallway.
    if ((bEnemyHallway && EnemySpawnCount != 1) || (!bEnemyHallway && EnemySpawnCount != 0))
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: Corridor %s needs %d EnemySpawn arrows for its assigned array."), *Candidate->GetName(), bEnemyHallway ? 1 : 0);
        Candidate->Destroy();
        return ContinueOrFail();
    }

    // Reject enemy hallways with a missing or ambiguous enemy type tag.
    const FName CandidateEnemyType = bEnemyHallway ? FindEnemyTypeForSpawnPoint(FindEnemySpawnPoint(Candidate)) : NAME_None;

    if (bEnemyHallway && CandidateEnemyType.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: Corridor %s needs exactly one enemy type tag assigned in EnemyClassesByTag."), *Candidate->GetName());
        Candidate->Destroy();
        return ContinueOrFail();
    }

    // Other valid enemy types stay available for their own placements.
    if (bEnemyHallway && CandidateEnemyType != PendingEnemyType)
    {
        Candidate->Destroy();
        return ContinueOrFail();
    }

    TArray<UArrowComponent*> Connections = SortedConnections(Candidate);
    TArray<FBox> Footprints;

    const bool bHasFootprints = GetFootprints(Candidate, Footprints);

    if (Connections.Num() < PendingMinConnections || Connections.Num() > PendingMaxConnections || !bHasFootprints)
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze: %s rejected. MazeConnection arrows: %d (required %d-%d). MazeBounds valid: %s."), *Candidate->GetName(), Connections.Num(), PendingMinConnections, PendingMaxConnections, bHasFootprints ? TEXT("true") : TEXT("false"));
        Candidate->Destroy();
        return ContinueOrFail();
    }

    // Use designated entrances when present. Otherwise, try every doorway.
    const bool bHasDesignatedEntrance = Connections.ContainsByPredicate([](UArrowComponent* Arrow)
        {
            return Arrow->ComponentHasTag(TEXT("MazeEntrance"));
        });

    TArray<int32> EntranceOrder;

    for (int32 Index = 0; Index < Connections.Num(); ++Index)
    {
        if (!bHasDesignatedEntrance || Connections[Index]->ComponentHasTag(TEXT("MazeEntrance")))
        {
            EntranceOrder.Add(Index);
        }
    }

    for (int32 Index = EntranceOrder.Num() - 1; Index > 0; --Index)
    {
        EntranceOrder.Swap(Index, RandomStream.RandRange(0, Index));
    }

    for (int32 Index : EntranceOrder)
    {
        Candidate->SetActorLocationAndRotation(PendingTargetPosition, FRotator::ZeroRotator);

        UArrowComponent* Arrow = Connections[Index];

        // Rotate the entrance against the previous exit and match the arrow bases.
        const float Yaw = FMath::UnwindDegrees(PendingTargetForward.Rotation().Yaw + 180.0f - Arrow->GetForwardVector().Rotation().Yaw);

        Candidate->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
        Candidate->SetActorLocation(Candidate->GetActorLocation() + PendingTargetPosition - Arrow->GetComponentLocation());

        if (FVector::DotProduct(Arrow->GetForwardVector(), PendingTargetForward.GetSafeNormal()) > -0.999f)
        {
            UE_LOG(LogTemp, Warning, TEXT("Maze: %s entrance %s does not face the previous exit."), *Candidate->GetName(), *Arrow->GetName());
            continue;
        }

        if (OverlapsMaze(Candidate, GenerationTarget.Get()))
        {
            continue;
        }

        bool bTouchesReservedOpening = false;

        for (UArrowComponent* Other : Connections)
        {
            if (Other != Arrow && (ConnectionsMatch(Other, StartArrow.Get()) || ConnectionsMatch(Other, EndArrow.Get())))
            {
                bTouchesReservedOpening = true;
                break;
            }
        }

        if (bTouchesReservedOpening)
        {
            UE_LOG(LogTemp, Warning, TEXT("Maze: %s reconnects to a reserved start/end opening."), *Candidate->GetName());
            continue;
        }

        // Try another entrance or piece before the main route reaches a crowded exit.
        if ((GenerationStage == EGenerationStage::MainRoute || GenerationStage == EGenerationStage::ExitHallways) && GetRouteExits(Candidate, Arrow).IsEmpty())
        {
            continue;
        }

        if (GenerationStage == EGenerationStage::PuzzleRoom)
        {
            UArrowComponent* Exit = FindTaggedConnection(Candidate, PuzzleRoomExitTag);

            if (IsValid(Exit) && Exit != Arrow && IsConnectionCrowded(Exit))
            {
                continue;
            }
        }

        // A new side path must leave space for a cap at its next opening.
        if (GenerationStage == EGenerationStage::Branches && !bCappingBranch && !CanReserveEndCaps(Candidate, Arrow, nullptr))
        {
            continue;
        }

        Piece = Candidate;
        Entrance = Arrow;
        bAttachmentPending = false;
        PendingAttachmentClasses.Reset();

        return EAttachmentResult::Succeeded;
    }

    Candidate->Destroy();

    return ContinueOrFail();
}

void AMazeGenerator::AddUnusedConnections(AMazeActors* Piece, UArrowComponent* Entrance, UArrowComponent* ReservedExit)
{
    // Track arrows that were not used by this attachment.
    for (UArrowComponent* Arrow : SortedConnections(Piece))
    {
        if (Arrow != Entrance && Arrow != ReservedExit)
        {
            OpenConnections.Add(Arrow);

            FBranchRoomState State = GenerationBranchRoomState;

            if (GenerationStage != EGenerationStage::Branches)
            {
                // Start a new side path with the room history at this junction.
                State.HallwaysSinceRoom = 0;
                State.LastRoomClass = GenerationLastRouteRoomClass.Get();
                State.ConsecutiveRooms = GenerationConsecutiveRouteRooms;
            }

            BranchRoomStates.Add(Arrow, State);
        }
    }
}

void AMazeGenerator::JoinMatchingOpenConnections()
{
    // Remove paired openings when their arrow positions and directions match.
    bool bJoined = true;

    while (bJoined)
    {
        bJoined = false;

        for (int32 A = 0; A < OpenConnections.Num() && !bJoined; ++A)
        {
            for (int32 B = A + 1; B < OpenConnections.Num(); ++B)
            {
                if (ConnectionsMatch(OpenConnections[A].Get(), OpenConnections[B].Get()))
                {
                    OpenConnections.RemoveAt(B);
                    OpenConnections.RemoveAt(A);

                    bJoined = true;
                    break;
                }
            }
        }
    }
}

void AMazeGenerator::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TRACE_CPUPROFILER_EVENT_SCOPE(MazeGenerationBatch);

    if (GenerationStage == EGenerationStage::Idle)
    {
        SetActorTickEnabled(false);
        return;
    }

    if (!GetWorld() || IsActorBeingDestroyed())
    {
        CancelGeneration();
        return;
    }

    if (!IsValid(CurrentRoom) && GenerationStage != EGenerationStage::Cleanup)
    {
        BeginGenerationCleanup(false);
    }

    const double StartTime = FPlatformTime::Seconds();
    const double BudgetSeconds = FMath::Clamp(GenerationTimeBudgetMs, 0.1f, 10.0f) / 1000.0;
    const int32 SpawnLimit = FMath::Clamp(MaxCandidateSpawnsPerFrame, 1, 16);
    const int32 ActiveSerial = GenerationSerial;

    CandidateSpawnsThisFrame = 0;
    int32 OperationsThisFrame = 0;

    // Tick is enabled only while a maze is being generated.
    while (GenerationStage != EGenerationStage::Idle && GenerationSerial == ActiveSerial && CandidateSpawnsThisFrame < SpawnLimit && OperationsThisFrame < 128)
    {
        if (FPlatformTime::Seconds() - StartTime >= BudgetSeconds)
        {
            break;
        }

        StepGeneration();
        ++OperationsThisFrame;

        // Poll navigation only once per frame; its rebuild runs asynchronously.
        if (GenerationStage == EGenerationStage::Navigation)
        {
            break;
        }
    }
}

bool AMazeGenerator::GenerateMaze()
{
    if (!GetWorld() || GenerationStage != EGenerationStage::Idle || LoopState != EMazeLoopState::Generating)
    {
        return false;
    }

    ++GenerationSerial;
    GenerationAttempt = 0;
    GenerationAttemptLimit = FMath::Clamp(LayoutAttempts, 1, 50);
    GenerationHallwayCount = FMath::Clamp(MaxHallwaysForward, 1, 100);
    GenerationExitHallwayCount = FMath::Clamp(HallwaysAfterPuzzle, 1, 20);
    GenerationMinJunctionSpacing = FMath::Clamp(MinHallwaysBetweenJunctions, 1, 100);
    GenerationMaxJunctionSpacing = FMath::Clamp(MaxHallwaysBetweenJunctions, GenerationMinJunctionSpacing, 100);
    GenerationBranchLimit = FMath::Clamp(MaxBranchHallways, 0, 100);
    EndCapFootprintCorners.Reset();

    BeginGenerationCleanup(true);
    SetActorTickEnabled(true);

    // The request was queued. The door stays closed until FinishGeneration.
    return true;
}

void AMazeGenerator::BeginGenerationCleanup(bool bRetry)
{
    // Temporary: report the stage before cleanup resets the failed layout.
    if (GenerationAttempt > 0 && GenerationStage != EGenerationStage::Idle && GenerationStage != EGenerationStage::Cleanup)
    {
        static const TCHAR* const StageNames[] =
        {
            TEXT("Idle"),
            TEXT("Cleanup"),
            TEXT("StartAttempt"),
            TEXT("MainRoute"),
            TEXT("PuzzleRoom"),
            TEXT("ExitHallways"),
            TEXT("EndRoom"),
            TEXT("NextRoom"),
            TEXT("Branches"),
            TEXT("Caps"),
            TEXT("PuzzleButtons"),
            TEXT("NavigationBounds"),
            TEXT("Navigation"),
            TEXT("Enemies"),
            TEXT("PuzzleEnemies"),
            TEXT("Complete")
        };

        const int32 StageIndex = static_cast<int32>(GenerationStage);

        UE_LOG(LogTemp, Warning, TEXT("Maze: Attempt %d/%d failed at %s. Puzzle: %s. Hallways: %d/%d. Route rooms: %d."), GenerationAttempt, GenerationAttemptLimit, StageNames[StageIndex], *ActivePuzzleId.ToString(), GenerationHallwayIndex, GenerationHallwayCount, GenerationRouteRoomCount);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChaserSpawnTimer);
    }

    ClearPendingArrival();

    bRetryAfterCleanup = bRetry;
    bAttachmentPending = false;
    PendingAttachmentClasses.Reset();
    BlockedConnections.Reset();
    GenerationPreviousExit.Reset();
    GenerationTarget.Reset();
    BranchRoomStates.Reset();
    GenerationBranchRoomState = FBranchRoomState();
    GenerationBranchRoomCount = 0;
    bTryingBranchRoom = false;
    bCappingBranch = false;

    OpenConnections.Reset();
    StartArrow.Reset();
    EndArrow.Reset();

    SpawnedEndRoom = nullptr;
    SpawnedPuzzleRoom = nullptr;
    bPuzzlePrepared = false;
    SpawnedEnemyHallways.Reset();
    SpawnedEnemyHallwayActors.Reset();
    NextRoom = nullptr;
    NextRoomEntranceTag = NAME_None;
    PendingEnemyType = NAME_None;
    GenerationRemainingEnemyTypes.Reset();
    GenerationPieceLinks.Reset();
    GenerationHallwayPieces.Reset();
    GenerationEnemySpawnIndex = 0;
    NavigationWaitStartedAt = 0.0;
    GeneratedMazeSize = FVector::ZeroVector;
    GeneratedNavBoundsSize = FVector::ZeroVector;

    bMainRouteComplete = false;
    UnfinishedBranchOpenings = 0;
    StartConnectionTransform = FTransform::Identity;
    EndConnectionTransform = FTransform::Identity;

    // Failed layouts are removed in small steps before the next attempt.
    GenerationStage = EGenerationStage::Cleanup;
}

void AMazeGenerator::StepGeneration()
{
    switch (GenerationStage)
    {
    case EGenerationStage::Cleanup:
        if (!SpawnedEnemies.IsEmpty())
        {
            const int32 Index = SpawnedEnemies.Num() - 1;
            APawn* Pawn = SpawnedEnemies[Index];
            SpawnedEnemies.RemoveAt(Index);

            if (IsValid(Pawn))
            {
                AController* Controller = Pawn->GetController();

                if (IsValid(Controller) && !Controller->IsPlayerController())
                {
                    Controller->Destroy();
                }

                Pawn->Destroy();
            }

            return;
        }

        if (!SpawnedMazeActors.IsEmpty())
        {
            const int32 Index = SpawnedMazeActors.Num() - 1;
            AMazeActors* Piece = SpawnedMazeActors[Index];
            SpawnedMazeActors.RemoveAt(Index);

            if (IsValid(Piece) && Piece != CurrentRoom)
            {
                Piece->Destroy();
            }

            return;
        }

        if (bRetryAfterCleanup && GenerationAttempt < GenerationAttemptLimit && IsValid(CurrentRoom))
        {
            GenerationStage = EGenerationStage::StartAttempt;
        }
        else
        {
            FinishGeneration(false);
        }

        return;

    case EGenerationStage::StartAttempt:
    {
        // This one preview spawn uses the same per-frame budget as other candidates.
        if (EndCapClass && EndCapFootprintCorners.IsEmpty())
        {
            if (!PrepareEndCapFootprint())
            {
                UE_LOG(LogTemp, Error, TEXT("Maze: EndCapClass needs one connection arrow and valid MazeBounds."));
                BeginGenerationCleanup(false);
            }

            return;
        }

        ++GenerationAttempt;
        GenerationHallwayIndex = 0;
        GenerationExitHallwayIndex = 0;
        GenerationRouteRoomCount = 0;
        GenerationHallwaysSinceRoom = 0;

        // Every attempt starts from the last successful maze's history.
        GenerationLastRouteRoomClass = LastRouteRoomClass;
        GenerationConsecutiveRouteRooms = ConsecutiveRouteRooms;
        if (!PrepareEnemyHallways())
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Could not prepare the shared enemy hallway counts."));
            BeginGenerationCleanup(false);
            return;
        }
        // Keep the puzzle away from the end. Short routes reduce the spacing as needed.
        const int32 LastPuzzleHallway = FMath::Max(1, GenerationHallwayCount - FMath::Clamp(MinHallwaysAfterPuzzle, 1, 100));
        const int32 FirstPuzzleHallway = FMath::Clamp(MinHallwaysBeforePuzzle, 1, LastPuzzleHallway);
        GenerationPuzzleHallwayIndex = ActivePuzzleRoomClass ? RandomStream.RandRange(FirstPuzzleHallway, LastPuzzleHallway) : 0;
        GenerationBranchCount = 0;
        GenerationCapIndex = -1;
        bTryingMainJunction = false;

        StartArrow = FindTaggedConnection(CurrentRoom, ChosenDoorTag);

        if (!StartArrow.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: The chosen door has no valid connection arrow."));
            BeginGenerationCleanup(false);
            return;
        }

        StartConnectionTransform = StartArrow->GetComponentTransform();
        GenerationPreviousExit = StartArrow;
        GenerationNextJunctionAt = RandomStream.RandRange(GenerationMinJunctionSpacing, GenerationMaxJunctionSpacing);
        GenerationStage = EGenerationStage::MainRoute;
        return;
    }

    case EGenerationStage::MainRoute:
        StepMainRoute();
        return;

    case EGenerationStage::PuzzleRoom:
    {
        if (!EndArrow.IsValid() || !ActivePuzzleRoomClass)
        {
            BeginGenerationCleanup(false);
            return;
        }

        if (!bAttachmentPending)
        {
            StartAttachment(EndArrow->GetComponentLocation(), EndArrow->GetForwardVector(), 2, 2, false, NAME_None, false, ActivePuzzleRoomClass);
        }

        AMazeActors* Piece = nullptr;
        UArrowComponent* Entrance = nullptr;
        const EAttachmentResult Result = StepAttachment(Piece, Entrance);

        if (Result == EAttachmentResult::Pending)
        {
            return;
        }

        if (Result == EAttachmentResult::Failed)
        {
            BeginGenerationCleanup(true);
            return;
        }

        UArrowComponent* Exit = FindTaggedConnection(Piece, PuzzleRoomExitTag);

        if (!IsValid(Exit) || Exit == Entrance || !Piece->FindComponentByClass<UMazePuzzleComponent>() || !Piece->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Each puzzle room needs MazePuzzleComponent, MazeInterface, and separate entrance/ToMazeExit connection arrows."));
            Piece->Destroy();
            BeginGenerationCleanup(false);
            return;
        }

        SpawnedPuzzleRoom = Piece;
        SpawnedMazeActors.Add(Piece);
        TrackMazeConnections(Piece, false);
        GenerationPreviousExit = Exit;
        EndArrow = Exit;
        EndConnectionTransform = Exit->GetComponentTransform();
        // A short route may already be finished when the puzzle is attached.
        GenerationStage = GenerationHallwayIndex == GenerationHallwayCount && !ShouldPlaceRouteRoom() ? EGenerationStage::ExitHallways : EGenerationStage::MainRoute;
        return;
    }

    case EGenerationStage::ExitHallways:
    {
        if (!EndArrow.IsValid())
        {
            BeginGenerationCleanup(true);
            return;
        }

        // Keep extra exit hallways ordinary. Enemies use the main-route and branch budgets.
        if (!bAttachmentPending)
        {
            StartAttachment(EndArrow->GetComponentLocation(), EndArrow->GetForwardVector(), 2, 2, false);
        }

        AMazeActors* Piece = nullptr;
        UArrowComponent* Entrance = nullptr;
        const EAttachmentResult Result = StepAttachment(Piece, Entrance);

        if (Result == EAttachmentResult::Pending)
        {
            return;
        }

        if (Result == EAttachmentResult::Failed)
        {
            BeginGenerationCleanup(true);
            return;
        }

        TArray<UArrowComponent*> Exits = GetRouteExits(Piece, Entrance);

        if (Exits.Num() != 1)
        {
            Piece->Destroy();
            BeginGenerationCleanup(true);
            return;
        }

        SpawnedMazeActors.Add(Piece);
        TrackMazeConnections(Piece, true);
        EndArrow = Exits[0];
        EndConnectionTransform = Exits[0]->GetComponentTransform();
        ++GenerationExitHallwayIndex;

        // Keep the existing bounds checks for BP_Exit and the next start room.
        if (GenerationExitHallwayIndex >= GenerationExitHallwayCount)
        {
            GenerationStage = EGenerationStage::EndRoom;
        }

        return;
    }

    case EGenerationStage::EndRoom:
        ++CandidateSpawnsThisFrame;

        if (SpawnEndRoom())
        {
            GenerationStage = EGenerationStage::NextRoom;
        }
        else
        {
            BeginGenerationCleanup(true);
        }

        return;

    case EGenerationStage::NextRoom:
        ++CandidateSpawnsThisFrame;

        if (SpawnNextRoom())
        {
            GenerationStage = EGenerationStage::Branches;
        }
        else
        {
            BeginGenerationCleanup(true);
        }

        return;

    case EGenerationStage::Branches:
        StepBranches();
        return;

    case EGenerationStage::Caps:
        StepCaps();
        return;

    case EGenerationStage::PuzzleButtons:
    {
        UMazePuzzleComponent* Puzzle = GetPuzzleComponent();

        if (!IsValid(Puzzle) || !IsValid(SpawnedEndRoom) || !SpawnedEndRoom->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Puzzle room and BP_Exit need MazeInterface for their door locks."));
            BeginGenerationCleanup(false);
            return;
        }

        if (!bPuzzlePrepared)
        {
            InitializeRoom(SpawnedPuzzleRoom);
            InitializeRoom(SpawnedEndRoom);
            IMazeInterface::Execute_SetMazeDoorLocked(SpawnedEndRoom, MazeEndDoorTag, true);

            if (!Puzzle->PreparePuzzle(this, ActivePuzzleId, EnemyClassesByTag.FindRef(WeepingAngelEnemyTag), RandomStream.RandRange(1, MAX_int32), bSpawnAllPuzzleButtons, bSpawnAllPuzzleWeepingAngels))
            {
                BeginGenerationCleanup(false);
                return;
            }

            bPuzzlePrepared = true;
        }

        bool bFinished = false;
        ++CandidateSpawnsThisFrame;

        if (!Puzzle->StepButtonSpawning(bFinished))
        {
            BeginGenerationCleanup(true);
        }
        else if (bFinished)
        {
            GenerationStage = EGenerationStage::NavigationBounds;
        }

        return;
    }

    // All pieces and caps are placed before the navigation volume is resized.
    case EGenerationStage::NavigationBounds:
        if (!IsValid(CurrentRoom) || !IsValid(NextRoom) || GenerationEnemyTypes.IsEmpty() || SpawnedEnemyHallwayActors.Num() != GenerationEnemyTypes.Num() || !UpdateMazeNavigationBounds())
        {
            BeginGenerationCleanup(false);
            return;
        }

        NavigationWaitStartedAt = FPlatformTime::Seconds();
        GenerationStage = EGenerationStage::Navigation;
        return;

    case EGenerationStage::Navigation:
        StepNavigation();
        return;

    case EGenerationStage::Enemies:
        if (GenerationEnemySpawnIndex < GenerationEnemyTypes.Num())
        {
            // A failed spawn removes all enemies and retries the layout.
            if (!SpawnEnemyInHallway(GenerationEnemySpawnIndex))
            {
                BeginGenerationCleanup(true);
                return;
            }

            ++GenerationEnemySpawnIndex;
        }

        if (GenerationEnemySpawnIndex == GenerationEnemyTypes.Num())
        {
            GenerationStage = PuzzleRoomsById.IsEmpty() ? EGenerationStage::Complete : EGenerationStage::PuzzleEnemies;
        }

        return;

    case EGenerationStage::PuzzleEnemies:
    {
        UMazePuzzleComponent* Puzzle = GetPuzzleComponent();
        bool bFinished = false;
        ++CandidateSpawnsThisFrame;

        if (!IsValid(Puzzle) || !Puzzle->StepEnemySpawning(bFinished))
        {
            BeginGenerationCleanup(true);
        }
        else if (bFinished)
        {
            GenerationStage = EGenerationStage::Complete;
        }

        return;
    }

    case EGenerationStage::Complete:
    {
        // A scheduled Chaser counts as ready without delaying the maze's doors.
        const bool bChaserPending = GetWorld()->GetTimerManager().IsTimerActive(ChaserSpawnTimer);
        bool bAllEnemiesValid = SpawnedEnemies.Num() == GenerationEnemyTypes.Num() && SpawnedEnemyHallwayActors.Num() == GenerationEnemyTypes.Num();

        for (int32 Index = 0; Index < SpawnedEnemyHallwayActors.Num(); ++Index)
        {
            AMazeActors* Hallway = SpawnedEnemyHallwayActors[Index];
            const FName EnemyType = FindEnemyTypeForSpawnPoint(FindEnemySpawnPoint(Hallway));
            const bool bEnemyReady = SpawnedEnemies.IsValidIndex(Index) && (IsValid(SpawnedEnemies[Index]) || (bChaserPending && EnemyType == ChaserEnemyTag));

            if (!IsValid(Hallway) || EnemyType.IsNone() || !GenerationEnemyTypes.Contains(EnemyType) || !bEnemyReady)
            {
                bAllEnemiesValid = false;
                break;
            }
        }

        if (!IsValid(CurrentRoom) || !IsValid(NextRoom) || GenerationEnemyTypes.IsEmpty() || !bAllEnemiesValid)
        {
            BeginGenerationCleanup(false);
            return;
        }

        FinishGeneration(true);
        return;
    }

    default:
        return;
    }
}

void AMazeGenerator::StepBranches()
{
    if (!bAttachmentPending)
    {
        TArray<int32> AvailableOpenings;

        for (int32 Index = 0; Index < OpenConnections.Num(); ++Index)
        {
            const FBranchRoomState State = BranchRoomStates.FindRef(OpenConnections[Index]);

            // A room due after the last branch hallway may still be placed.
            if (OpenConnections[Index].IsValid() && (GenerationBranchCount < GenerationBranchLimit || ShouldPlaceBranchRoom(State)))
            {
                AvailableOpenings.Add(Index);
            }
        }

        if (AvailableOpenings.IsEmpty())
        {
            if (!ValidateEnemyHallwaySpacing())
            {
                BeginGenerationCleanup(true);
                return;
            }

            OpenConnections.Append(BlockedConnections);
            BlockedConnections.Reset();
            JoinMatchingOpenConnections();

            bTryingBranchRoom = false;
            GenerationCapIndex = OpenConnections.Num() - 1;
            GenerationStage = EGenerationStage::Caps;
            return;
        }

        const int32 Index = AvailableOpenings[RandomStream.RandRange(0, AvailableOpenings.Num() - 1)];
        GenerationTarget = OpenConnections[Index];
        GenerationBranchRoomState = BranchRoomStates.FindRef(GenerationTarget);
        BranchRoomStates.Remove(GenerationTarget);
        OpenConnections.RemoveAt(Index);

        UArrowComponent* Target = GenerationTarget.Get();
        bCappingBranch = IsConnectionCrowded(Target);
        bTryingBranchRoom = !bCappingBranch && ShouldPlaceBranchRoom(GenerationBranchRoomState);

        const FName EnemyType = !bCappingBranch && !bTryingBranchRoom && bAllowEnemyHallwaysOnBranches ? ChooseEnemyHallwayType(Target, GenerationBranchLimit - GenerationBranchCount) : NAME_None;

        // A crowded side opening closes here instead of growing towards another piece.
        StartAttachment(Target->GetComponentLocation(), Target->GetForwardVector(), bCappingBranch ? 1 : 2, bCappingBranch ? 1 : 2, bCappingBranch, EnemyType, bTryingBranchRoom);
    }

    if (!GenerationTarget.IsValid())
    {
        bAttachmentPending = false;
        bTryingBranchRoom = false;
        bCappingBranch = false;
        PendingAttachmentClasses.Reset();
        return;
    }

    const FName EnemyType = PendingEnemyType;
    UArrowComponent* Entrance = nullptr;
    AMazeActors* Piece = nullptr;
    const EAttachmentResult Result = StepAttachment(Piece, Entrance);

    if (Result == EAttachmentResult::Pending)
    {
        return;
    }

    if (Result == EAttachmentResult::Failed)
    {
        if (!EnemyType.IsNone())
        {
            // A rejected enemy piece does not lose its count or immediately end the branch.
            UArrowComponent* Target = GenerationTarget.Get();
            StartAttachment(Target->GetComponentLocation(), Target->GetForwardVector(), 2, 2, false);
            return;
        }

        if (!bCappingBranch)
        {
            // Keep the existing path and close this opening if no extension can fit.
            bTryingBranchRoom = false;
            bCappingBranch = true;
            UArrowComponent* Target = GenerationTarget.Get();
            StartAttachment(Target->GetComponentLocation(), Target->GetForwardVector(), 1, 1, true);
            return;
        }

        // A cap still has to pass the real bounds check before accepting this layout.
        BeginGenerationCleanup(true);
        return;
    }
    else
    {
        SpawnedMazeActors.Add(Piece);
        TrackMazeConnections(Piece, !bCappingBranch && !bTryingBranchRoom);

        if (!EnemyType.IsNone() && !RegisterEnemyHallway(Piece, EnemyType))
        {
            BeginGenerationCleanup(true);
            return;
        }

        if (bCappingBranch)
        {
            bCappingBranch = false;
            GenerationTarget.Reset();
            return;
        }

        if (bTryingBranchRoom)
        {
            ++GenerationBranchRoomCount;
            GenerationBranchRoomState.HallwaysSinceRoom = 0;
            GenerationBranchRoomState.ConsecutiveRooms = Piece->GetClass() == GenerationBranchRoomState.LastRoomClass.Get() ? GenerationBranchRoomState.ConsecutiveRooms + 1 : 1;
            GenerationBranchRoomState.LastRoomClass = Piece->GetClass();
        }
        else
        {
            ++GenerationBranchCount;
            ++GenerationBranchRoomState.HallwaysSinceRoom;
        }

        AddUnusedConnections(Piece, Entrance, nullptr);

        OpenConnections.Append(BlockedConnections);
        BlockedConnections.Reset();
        JoinMatchingOpenConnections();
    }

    bTryingBranchRoom = false;
    GenerationTarget.Reset();
}

void AMazeGenerator::StepCaps()
{
    if (!bAttachmentPending)
    {
        if (GenerationCapIndex < 0)
        {
            UnfinishedBranchOpenings = OpenConnections.Num();

            if (UnfinishedBranchOpenings > 0)
            {
                if (!EndCapClass)
                {
                    UE_LOG(LogTemp, Error, TEXT("Maze: Side openings require an assigned EndCapClass."));
                    BeginGenerationCleanup(false);
                }
                else
                {
                    BeginGenerationCleanup(true);
                }

                return;
            }

            // Finish only after the main route, destination room, and all caps succeed.
            bMainRouteComplete = true;
            GenerationStage = PuzzleRoomsById.IsEmpty() ? EGenerationStage::NavigationBounds : EGenerationStage::PuzzleButtons;
            return;
        }

        UArrowComponent* Target = OpenConnections[GenerationCapIndex].Get();

        if (!IsValid(Target))
        {
            OpenConnections.RemoveAt(GenerationCapIndex--);
            return;
        }

        if (!EndCapClass)
        {
            --GenerationCapIndex;
            return;
        }

        GenerationTarget = Target;
        StartAttachment(Target->GetComponentLocation(), Target->GetForwardVector(), 1, 1, true);
    }

    if (!GenerationTarget.IsValid())
    {
        bAttachmentPending = false;
        PendingAttachmentClasses.Reset();
        OpenConnections.RemoveAt(GenerationCapIndex--);
        return;
    }

    UArrowComponent* Entrance = nullptr;
    AMazeActors* Cap = nullptr;
    const EAttachmentResult Result = StepAttachment(Cap, Entrance);

    if (Result == EAttachmentResult::Pending)
    {
        return;
    }

    if (Result == EAttachmentResult::Succeeded)
    {
        SpawnedMazeActors.Add(Cap);
        OpenConnections.RemoveAt(GenerationCapIndex);
    }

    --GenerationCapIndex;
    GenerationTarget.Reset();
}

void AMazeGenerator::FinishGeneration(bool bSuccess)
{
    AMazeActors* CompletedRoom = CurrentRoom;
    const FName CompletedDoorTag = ChosenDoorTag;
    const TArray<FName> PreviousDoors = PreviousUnlockedDoorTags;

    // Commit room history only when the whole maze is accepted.
    if (bSuccess)
    {
        RouteRoomClassBeforeMaze = LastRouteRoomClass;
        ConsecutiveRouteRoomsBeforeMaze = ConsecutiveRouteRooms;
        LastRouteRoomClass = GenerationLastRouteRoomClass;
        ConsecutiveRouteRooms = GenerationConsecutiveRouteRooms;
    }

    CancelGeneration();

    if (bSuccess)
    {
        InitializeRoom(NextRoom);

        LoopState = EMazeLoopState::TraversingMaze;

        if (UMazePuzzleComponent* Puzzle = GetPuzzleComponent())
        {
            Puzzle->ArmPuzzle();
        }

        UnlockedDoorTags.Reset();
        UnlockedDoorTags.Add(ChosenDoorTag);
        ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

        TArray<FName> DestinationDoors;
        DestinationDoors.Add(NextRoomEntranceTag);
        ApplyDoorLocks(NextRoom, DestinationDoors);

        UE_LOG(LogTemp, Display, TEXT("Maze: %d main-route hallways and %d branch hallways generated, including %d enemy hallways across both paths. Navigation ready; all side openings closed."), GenerationHallwayCount, GenerationBranchCount, SpawnedEnemyHallwayActors.Num());
    }
    else
    {
        ChosenDoorTag = NAME_None;
        UnlockedDoorTags = PreviousDoors;
        LoopState = EMazeLoopState::WaitingForDoor;
        ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

        UE_LOG(LogTemp, Error, TEXT("Maze: Could not generate a complete route with all side openings capped. Check piece bounds or layout settings."));
    }

    // Blueprints may open the chosen door now, after the maze and locks are ready.
    OnMazeGenerationFinished.Broadcast(CompletedRoom, CompletedDoorTag, bSuccess);
}

void AMazeGenerator::CancelGeneration()
{
    ++GenerationSerial;
    GenerationStage = EGenerationStage::Idle;
    SetActorTickEnabled(false);

    bAttachmentPending = false;
    PendingEnemyType = NAME_None;
    GenerationEnemyTypes.Reset();
    GenerationRemainingEnemyTypes.Reset();
    GenerationPieceLinks.Reset();
    GenerationHallwayPieces.Reset();
    GenerationEnemySpawnIndex = 0;

    // Discard tentative history while keeping the last successful maze's history.
    GenerationLastRouteRoomClass = nullptr;
    GenerationConsecutiveRouteRooms = 0;
    PendingAttachmentClasses.Reset();
    PreviousUnlockedDoorTags.Reset();
    BlockedConnections.Reset();
    GenerationPreviousExit.Reset();
    GenerationTarget.Reset();
    BranchRoomStates.Reset();
    GenerationBranchRoomState = FBranchRoomState();
    GenerationBranchRoomCount = 0;
    bTryingBranchRoom = false;
    bCappingBranch = false;
    EndCapFootprintCorners.Reset();
}

void AMazeGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChaserSpawnTimer);
    }

    CancelGeneration();

    ClearPendingArrival();

    Super::EndPlay(EndPlayReason);
}

bool AMazeGenerator::ValidateRoom(AMazeActors* Room, FName IncomingDoorTag) const
{
    // Show the actual runtime room and map when a check fails.
    const auto LogFailure = [this, Room](const FString& Reason)
        {
            UE_LOG(LogTemp, Error, TEXT("Maze validation: %s Generator: %s. Room: %s. Map: %s."), *Reason, *GetName(), IsValid(Room) ? *Room->GetPathName() : TEXT("None or invalid"), GetWorld() ? *GetWorld()->GetMapName() : TEXT("None"));
            return false;
        };

    if (!IsValid(Room))
    {
        return LogFailure(TEXT("The assigned start-room actor is missing or invalid."));
    }

    if (DoorTags.Num() != 3)
    {
        return LogFailure(FString::Printf(TEXT("DoorTags contains %d entries. Exactly three are required."), DoorTags.Num()));
    }

    if (!Room->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()))
    {
        return LogFailure(FString::Printf(TEXT("Room class %s does not implement MazeInterface."), *Room->GetClass()->GetPathName()));
    }

    TArray<FName> RequiredDoorTags = DoorTags;

    if (!IncomingDoorTag.IsNone())
    {
        // The duplicate has a fourth doorway, separate from the three maze choices.
        if (DoorTags.Contains(IncomingDoorTag))
        {
            return LogFailure(FString::Printf(TEXT("Incoming door tag %s is also an outgoing DoorTags entry."), *IncomingDoorTag.ToString()));
        }

        RequiredDoorTags.Add(IncomingDoorTag);
    }

    // Every required doorway needs its own tagged connection arrow.
    TSet<FName> UniqueTags;
    TSet<UArrowComponent*> UniqueArrows;

    const TArray<UArrowComponent*> RoomConnections = SortedConnections(Room);

    for (const FName& DoorTag : RequiredDoorTags)
    {
        UArrowComponent* Arrow = FindTaggedConnection(Room, DoorTag);
        int32 MatchingArrows = 0;

        for (UArrowComponent* Connection : RoomConnections)
        {
            if (Connection->ComponentHasTag(DoorTag))
            {
                ++MatchingArrows;
            }
        }

        if (DoorTag.IsNone())
        {
            return LogFailure(TEXT("DoorTags contains an empty tag."));
        }

        if (UniqueTags.Contains(DoorTag))
        {
            return LogFailure(FString::Printf(TEXT("Door tag %s appears more than once in DoorTags."), *DoorTag.ToString()));
        }

        if (MatchingArrows != 1)
        {
            return LogFailure(FString::Printf(TEXT("Door tag %s matches %d connection arrows. Exactly one is required. Eligible connection arrows: %d. Check MazeConnection and the door tag on the arrow, and keep EnemySpawn and PlayerSpawn off door arrows."), *DoorTag.ToString(), MatchingArrows, RoomConnections.Num()));
        }

        if (!IsValid(Arrow) || UniqueArrows.Contains(Arrow))
        {
            return LogFailure(FString::Printf(TEXT("Door tag %s has no valid separate arrow. The same arrow cannot serve two doors."), *DoorTag.ToString()));
        }

        UniqueTags.Add(DoorTag);
        UniqueArrows.Add(Arrow);
    }

    // The arrival trigger must be separate from the MazeBounds boxes.
    int32 ArrivalTriggerCount = 0;

    TArray<UBoxComponent*> Boxes;
    Room->GetComponents<UBoxComponent>(Boxes);

    for (UBoxComponent* Box : Boxes)
    {
        if (IsValid(Box) && Box->ComponentHasTag(TEXT("RoomArrival")))
        {
            if (Box->ComponentHasTag(TEXT("MazeBounds")))
            {
                return LogFailure(FString::Printf(TEXT("Box %s has both RoomArrival and MazeBounds. Use separate boxes."), *Box->GetName()));
            }

            ++ArrivalTriggerCount;
        }
    }

    if (ArrivalTriggerCount != 1)
    {
        return LogFailure(FString::Printf(TEXT("Found %d box components tagged RoomArrival. Exactly one is required."), ArrivalTriggerCount));
    }

    TArray<FBox> Footprints;

    if (!GetFootprints(Room, Footprints))
    {
        return LogFailure(TEXT("The room has no usable MazeBounds box. Check the preceding bounds warning for its name and extent."));
    }

    return true;
}

UBoxComponent* AMazeGenerator::FindArrivalTrigger(AMazeActors* Room) const
{
    if (!IsValid(Room))
    {
        return nullptr;
    }

    TArray<UBoxComponent*> Boxes;
    Room->GetComponents<UBoxComponent>(Boxes);

    for (UBoxComponent* Box : Boxes)
    {
        if (IsValid(Box) && Box->ComponentHasTag(TEXT("RoomArrival")))
        {
            return Box;
        }
    }

    return nullptr;
}

void AMazeGenerator::InitializeRoom(AMazeActors* Room)
{
    // Give the room Blueprint its generator reference.
    IMazeInterface::Execute_InitializeMazeRoom(Room, this);

    UBoxComponent* Trigger = FindArrivalTrigger(Room);

    if (IsValid(Trigger))
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMazeGenerator::OnRoomArrivalBeginOverlap);
        Trigger->OnComponentEndOverlap.AddUniqueDynamic(this, &AMazeGenerator::OnRoomArrivalEndOverlap);

        Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
        Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        Trigger->SetGenerateOverlapEvents(true);
    }
}

void AMazeGenerator::ApplyDoorLocks(AMazeActors* Room, const TArray<FName>& AllowedDoors)
{
    if (!IsValid(Room) || !Room->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()))
    {
        return;
    }

    for (const FName& DoorTag : DoorTags)
    {
        IMazeInterface::Execute_SetMazeDoorLocked(Room, DoorTag, !AllowedDoors.Contains(DoorTag));
    }

    // Door4 is an incoming connection, so it is not part of the three choice tags.
    const FName IncomingDoorTag = Room == CurrentRoom ? CurrentRoomEntranceTag : (Room == NextRoom ? NextRoomEntranceTag : NAME_None);

    if (!IncomingDoorTag.IsNone() && !DoorTags.Contains(IncomingDoorTag))
    {
        IMazeInterface::Execute_SetMazeDoorLocked(Room, IncomingDoorTag, !AllowedDoors.Contains(IncomingDoorTag));
    }
}

bool AMazeGenerator::IsDoorUnlocked(AMazeActors* Room, FName DoorTag) const
{
    if (!IsValid(Room) || DoorTag.IsNone())
    {
        return false;
    }

    if (LoopState == EMazeLoopState::WaitingForDoor && Room == CurrentRoom)
    {
        return DoorTags.Contains(DoorTag) && DoorTag != CurrentRoomEntranceTag && UnlockedDoorTags.Contains(DoorTag);
    }

    if (LoopState == EMazeLoopState::TraversingMaze)
    {
        if (!PuzzleRoomsById.IsEmpty() && Room == SpawnedEndRoom && DoorTag == MazeEndDoorTag)
        {
            return bCurrentPuzzleCompleted;
        }

        UMazePuzzleComponent* Puzzle = GetPuzzleComponent();

        if (Room == SpawnedPuzzleRoom && IsValid(Puzzle) && DoorTag == Puzzle->PuzzleDoorTag)
        {
            return Puzzle->bPuzzleDoorUnlocked;
        }

        return (Room == CurrentRoom && DoorTag == ChosenDoorTag && UnlockedDoorTags.Contains(DoorTag)) || (Room == NextRoom && DoorTag == NextRoomEntranceTag);
    }

    return false;
}

bool AMazeGenerator::SetDoorUnlocked(AMazeActors* Room, FName DoorTag, bool bUnlocked)
{
    if (LoopState != EMazeLoopState::WaitingForDoor || !IsValid(Room) || Room != CurrentRoom || !DoorTags.Contains(DoorTag) || DoorTag == CurrentRoomEntranceTag)
    {
        return false;
    }

    if (bUnlocked)
    {
        UnlockedDoorTags.AddUnique(DoorTag);
    }
    else
    {
        UnlockedDoorTags.Remove(DoorTag);
    }

    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

    if (UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this))
    {
        SaveSystem->UpdateCheckpoint();
    }

    return true;
}

bool AMazeGenerator::ChooseDoor(AMazeActors* Room, FName DoorTag)
{
    // Reopening the route's two endpoint doors never generates another maze.
    if (LoopState == EMazeLoopState::TraversingMaze)
    {
        return IsDoorUnlocked(Room, DoorTag);
    }

    if (LoopState != EMazeLoopState::WaitingForDoor || !IsDoorUnlocked(Room, DoorTag))
    {
        return false;
    }

    const bool bValidNextRoomDoors = !NextRoomEntranceDoorTag.IsNone() && !DoorTags.Contains(NextRoomEntranceDoorTag) && NextRoomUnlockedDoorTags.Num() == 2 && NextRoomUnlockedDoorTags[0] != NextRoomUnlockedDoorTags[1] && DoorTags.Contains(NextRoomUnlockedDoorTags[0]) && DoorTags.Contains(NextRoomUnlockedDoorTags[1]);

    if (!GetWorld() || !ValidateRoom(CurrentRoom, CurrentRoomEntranceTag) || CorridorClasses.IsEmpty() || !EndRoomClass || !NextStartRoomClass || EndRoomExitTag.IsNone() || !bValidNextRoomDoors)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Assign corridor and room classes, EndRoomExitTag, a separate incoming door tag (Door4), and exactly two distinct choice tags in NextRoomUnlockedDoorTags."));
        return false;
    }

    // Reject missing room classes and enemy hallways assigned as rooms.
    for (const TSubclassOf<AMazeActors>& RoomClass : RouteRoomClasses)
    {
        if (!RoomClass || IsEnemyCorridorClass(RoomClass))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze: Each RouteRoomClasses entry needs a separate card, trap, or puzzle room class."));
            return false;
        }
    }

    const bool bHasNormalCorridor = CorridorClasses.ContainsByPredicate([this](const TSubclassOf<AMazeActors>& PieceClass)
        {
            return PieceClass && !IsEnemyCorridorClass(PieceClass) && !IsRouteRoomClass(PieceClass);
        });

    const bool bHasEnemyCorridor = EnemyCorridorClasses.ContainsByPredicate([](const TSubclassOf<AMazeActors>& PieceClass)
        {
            return PieceClass.Get() != nullptr;
        });

    if (!bHasNormalCorridor || !bHasEnemyCorridor || !IsValid(MazeNavBounds) || !PrepareEnemyTypes())
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: Assign ordinary CorridorClasses, separate EnemyCorridorClasses, EnemyClassesByTag, and the level's MazeNavBounds volume."));
        return false;
    }

    if (!SelectPuzzleRoom())
    {
        return false;
    }

    PreviousUnlockedDoorTags = UnlockedDoorTags;
    ChosenDoorTag = DoorTag;
    LoopState = EMazeLoopState::Generating;

    // Keep every door closed while generation continues across frames.
    ApplyDoorLocks(CurrentRoom, TArray<FName>{});

    if (!GenerateMaze())
    {
        ChosenDoorTag = NAME_None;
        UnlockedDoorTags = PreviousUnlockedDoorTags;
        PreviousUnlockedDoorTags.Reset();
        LoopState = EMazeLoopState::WaitingForDoor;
        ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);
    }

    // A new request cannot open yet. OnMazeGenerationFinished handles its opening.
    return false;
}

// Reuse the selected door's normal generation flow when testing a new layout.
bool AMazeGenerator::RebuildMazeAtChosenDoor()
{
    if (GenerationStage != EGenerationStage::Idle || LoopState != EMazeLoopState::TraversingMaze || !IsValid(CurrentRoom) || ChosenDoorTag.IsNone())
    {
        return false;
    }

    const FName DoorToRebuild = ChosenDoorTag;
    const TArray<FName> SavedUnlockedDoors = UnlockedDoorTags;
    const TSubclassOf<AMazeActors> SavedRoomClass = LastRouteRoomClass;
    const int32 SavedRoomCount = ConsecutiveRouteRooms;
    const TArray<FName> SavedCompletedPuzzles = CompletedPuzzleIds;
    const bool bSavedPuzzleCompleted = bCurrentPuzzleCompleted;

    // A test rebuild repeats this puzzle without consuming another completion.
    CompletedPuzzleIds.Remove(ActivePuzzleId);
    bCurrentPuzzleCompleted = false;
    bRebuildingPuzzle = true;

    // Replacing a layout should not count its rooms a second time.
    LastRouteRoomClass = RouteRoomClassBeforeMaze;
    ConsecutiveRouteRooms = ConsecutiveRouteRoomsBeforeMaze;

    // Allow the same door to be chosen again, even if it closed behind the player.
    LoopState = EMazeLoopState::WaitingForDoor;
    UnlockedDoorTags.AddUnique(DoorToRebuild);
    ChooseDoor(CurrentRoom, DoorToRebuild);
    bRebuildingPuzzle = false;

    if (LoopState == EMazeLoopState::Generating)
    {
        return true;
    }

    // Keep the current layout and locks if its settings were rejected.
    CompletedPuzzleIds = SavedCompletedPuzzles;
    bCurrentPuzzleCompleted = bSavedPuzzleCompleted;
    LastRouteRoomClass = SavedRoomClass;
    ConsecutiveRouteRooms = SavedRoomCount;
    ChosenDoorTag = DoorToRebuild;
    UnlockedDoorTags = SavedUnlockedDoors;
    LoopState = EMazeLoopState::TraversingMaze;
    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

    return false;
}

void AMazeGenerator::OnRoomArrivalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* Player = Cast<APawn>(OtherActor);
    UBoxComponent* Trigger = Cast<UBoxComponent>(OverlappedComponent);

    if (LoopState != EMazeLoopState::TraversingMaze || !IsValid(NextRoom) || !IsValid(Player) || !Player->IsPlayerControlled() || !IsValid(Trigger) || Trigger->GetOwner() != NextRoom)
    {
        return;
    }

    if (PlayerEnteredNextRoom(NextRoom, Player))
    {
        return;
    }

    // A box may overlap before the entire capsule has cleared the entrance.
    PendingArrivalTrigger = Trigger;
    PendingArrivalPlayer = Player;

    GetWorld()->GetTimerManager().SetTimer(ArrivalCheckTimer, this, &AMazeGenerator::CheckPendingArrival, 0.05f, true);
}

void AMazeGenerator::OnRoomArrivalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (OverlappedComponent == PendingArrivalTrigger.Get() && OtherActor == PendingArrivalPlayer.Get())
    {
        // Other collision components may still overlap the box; the timer checks that.
        CheckPendingArrival();
    }
}

void AMazeGenerator::CheckPendingArrival()
{
    UBoxComponent* Trigger = PendingArrivalTrigger.Get();
    APawn* Player = PendingArrivalPlayer.Get();

    if (LoopState != EMazeLoopState::TraversingMaze || !IsValid(NextRoom) || !IsValid(Trigger) || !IsValid(Player) || Trigger->GetOwner() != NextRoom || !Trigger->IsOverlappingActor(Player))
    {
        ClearPendingArrival();
        return;
    }

    PlayerEnteredNextRoom(NextRoom, Player);
}

void AMazeGenerator::ClearPendingArrival()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ArrivalCheckTimer);
    }

    PendingArrivalTrigger.Reset();
    PendingArrivalPlayer.Reset();
}

bool AMazeGenerator::HasPlayerClearedEntrance(APawn* Player) const
{
    UArrowComponent* Entrance = FindTaggedConnection(NextRoom, NextRoomEntranceTag);

    if (!IsValid(Player) || !IsValid(Entrance))
    {
        return false;
    }

    // Door arrows point outward. Interior positions have negative distance.
    const FVector Outward = Entrance->GetForwardVector().GetSafeNormal2D();

    if (Outward.IsNearlyZero())
    {
        return false;
    }

    FVector BoundsOrigin;
    FVector BoundsExtent;

    // Check the full colliding bounds before removing the maze behind the player.
    Player->GetActorBounds(true, BoundsOrigin, BoundsExtent);

    const double CenterDistance = FVector::DotProduct(BoundsOrigin - Entrance->GetComponentLocation(), Outward);
    const double ProjectedExtent = FVector::DotProduct(BoundsExtent, Outward.GetAbs());

    return CenterDistance + ProjectedExtent <= -FMath::Max(0.0f, ArrivalClearance);
}

bool AMazeGenerator::PlayerEnteredNextRoom(AMazeActors* Room, APawn* Player)
{
    if (LoopState != EMazeLoopState::TraversingMaze || !IsValid(Room) || Room != NextRoom || !IsValid(Player) || !Player->IsPlayerControlled() || !HasPlayerClearedEntrance(Player))
    {
        return false;
    }

    // Escaping cannot advance to another door until this maze's puzzle is solved.
    if (!PuzzleRoomsById.IsEmpty() && !bCurrentPuzzleCompleted)
    {
        return false;
    }

    UBoxComponent* Trigger = FindArrivalTrigger(Room);

    if (!IsValid(Trigger) || !Trigger->IsOverlappingActor(Player))
    {
        return false;
    }

    LoopState = EMazeLoopState::ChangingRooms;

    AMazeActors* PreviousRoom = CurrentRoom;
    AMazeActors* ArrivedRoom = NextRoom;
    const FName ArrivedEntrance = NextRoomEntranceTag;

    // Close and block the incoming doorway before removing the floor behind it.
    ApplyDoorLocks(ArrivedRoom, TArray<FName>{});

    // Make the arrived room the current room before clearing the layout.
    SpawnedMazeActors.Remove(ArrivedRoom);

    CurrentRoom = ArrivedRoom;
    CurrentRoomEntranceTag = ArrivedEntrance;

    ClearGeneratedLayout();

    if (InitialStartRoom == PreviousRoom)
    {
        InitialStartRoom = nullptr;
    }

    if (IsValid(PreviousRoom))
    {
        PreviousRoom->Destroy();
    }

    ChosenDoorTag = NAME_None;

    // Each escape unlocks another choice and keeps the earlier choices available.
    // The old two-choice behavior remains available when no puzzle rooms are assigned.
    const bool bAllPuzzlesEscaped = !PuzzleRoomsById.IsEmpty() && CompletedPuzzleIds.Num() == 3;
    UnlockedDoorTags = NextRoomUnlockedDoorTags;

    if (!PuzzleRoomsById.IsEmpty())
    {
        UnlockedDoorTags.Reset();

        if (!bAllPuzzlesEscaped)
        {
            for (int32 DoorIndex = 0; DoorIndex <= CompletedPuzzleIds.Num() && DoorIndex < DoorTags.Num(); ++DoorIndex)
            {
                UnlockedDoorTags.AddUnique(DoorTags[DoorIndex]);
            }
        }
    }

    ActivePuzzleId = NAME_None;
    ActivePuzzleRoomClass = nullptr;
    bCurrentPuzzleCompleted = false;
    ++RoomsCompleted;
    LoopState = bAllPuzzlesEscaped ? EMazeLoopState::Completed : EMazeLoopState::WaitingForDoor;
    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

    if (UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this))
    {
        SaveSystem->UpdateCheckpoint();
    }

    if (bAllPuzzlesEscaped)
    {
        OnAllPuzzlesEscaped.Broadcast();
    }

    return true;
}

bool AMazeGenerator::PlayerEnteredMaze(AMazeActors* Room, FName DoorTag, APawn* Player)
{
    if (LoopState != EMazeLoopState::TraversingMaze || !IsValid(Room) || Room != CurrentRoom || DoorTag != ChosenDoorTag || !IsDoorUnlocked(Room, DoorTag) || !IsValid(Player) || !Player->IsPlayerControlled())
    {
        return false;
    }

    UArrowComponent* Exit = FindTaggedConnection(Room, DoorTag);

    if (!IsValid(Exit))
    {
        return false;
    }

    // Start-room arrows point outward into the maze.
    const FVector Outward = Exit->GetForwardVector().GetSafeNormal2D();

    if (Outward.IsNearlyZero())
    {
        return false;
    }

    FVector BoundsOrigin;
    FVector BoundsExtent;

    Player->GetActorBounds(true, BoundsOrigin, BoundsExtent);

    const double CenterDistance = FVector::DotProduct(BoundsOrigin - Exit->GetComponentLocation(), Outward);
    const double ProjectedExtent = FVector::DotProduct(BoundsExtent, Outward.GetAbs());

    // Walking away on the room side must not lock the chosen door.
    if (CenterDistance - ProjectedExtent < FMath::Max(0.0f, DepartureClearance))
    {
        return false;
    }

    // The player has entered the maze. This door cannot reopen during this run.
    UnlockedDoorTags.Remove(DoorTag);
    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);

    return true;
}

// Attach the duplicate start room's Door4 to the end room's tagged exit.
bool AMazeGenerator::SpawnNextRoom()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MazeSpawnNextRoom);

    if (!NextStartRoomClass || !IsValid(SpawnedEndRoom))
    {
        return false;
    }

    UArrowComponent* RoomExit = FindTaggedConnection(SpawnedEndRoom, EndRoomExitTag);

    if (!IsValid(RoomExit))
    {
        return false;
    }

    const FVector TargetPosition = RoomExit->GetComponentLocation();
    const FVector TargetForward = RoomExit->GetForwardVector();

    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMazeActors* Candidate = GetWorld()->SpawnActor<AMazeActors>(NextStartRoomClass, TargetPosition, FRotator::ZeroRotator, Parameters);

    if (!IsValid(Candidate))
    {
        return false;
    }

    MakeMovable(Candidate);

    if (!ValidateRoom(Candidate, NextRoomEntranceDoorTag))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze: NextStartRoomClass needs MazeInterface, three choice arrows, a separate incoming arrow tagged %s, MazeBounds, and one RoomArrival box."), *NextRoomEntranceDoorTag.ToString());

        Candidate->Destroy();
        return false;
    }

    UArrowComponent* Entrance = FindTaggedConnection(Candidate, NextRoomEntranceDoorTag);

    if (!IsValid(Entrance))
    {
        Candidate->Destroy();
        return false;
    }

    const float Yaw = FMath::UnwindDegrees(TargetForward.Rotation().Yaw + 180.0f - Entrance->GetForwardVector().Rotation().Yaw);

    Candidate->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
    Candidate->SetActorLocation(Candidate->GetActorLocation() + TargetPosition - Entrance->GetComponentLocation());

    const bool bFacingCorrectly = FVector::DotProduct(Entrance->GetForwardVector(), TargetForward.GetSafeNormal()) <= -0.999f;
    const bool bPositionMatches = FVector::Dist(Entrance->GetComponentLocation(), TargetPosition) <= FMath::Max(0.1f, ConnectionTolerance);

    if (!bFacingCorrectly || !bPositionMatches || OverlapsMaze(Candidate))
    {
        Candidate->Destroy();
        return false;
    }

    NextRoom = Candidate;
    NextRoomEntranceTag = NextRoomEntranceDoorTag;

    SpawnedMazeActors.Add(Candidate);
    TrackMazeConnections(Candidate, false);

    return true;
}

// Capture only committed progress from a start room before the next maze is chosen.
bool AMazeGenerator::CaptureStartRoomState(FMazeStartRoomSaveData& Data) const
{
    if (GenerationStage != EGenerationStage::Idle || LoopState != EMazeLoopState::WaitingForDoor || !IsValid(CurrentRoom))
    {
        return false;
    }

    TArray<UArrowComponent*> Arrows;
    CurrentRoom->GetComponents<UArrowComponent>(Arrows);
    UArrowComponent* Respawn = nullptr;

    for (UArrowComponent* Arrow : Arrows)
    {
        if (!IsValid(Arrow) || !Arrow->ComponentHasTag(PlayerRespawnTag))
        {
            continue;
        }

        if (IsValid(Respawn))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze save: %s needs exactly one arrow tagged %s."), *CurrentRoom->GetName(), *PlayerRespawnTag.ToString());
            return false;
        }

        Respawn = Arrow;
    }

    if (!IsValid(Respawn))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze save: Add one arrow tagged %s inside %s, with room for the player capsule."), *PlayerRespawnTag.ToString(), *CurrentRoom->GetName());
        return false;
    }

    Data = FMazeStartRoomSaveData();
    Data.RoomClass = TSoftClassPtr<AMazeActors>(CurrentRoom->GetClass());
    Data.RoomTransform = CurrentRoom->GetActorTransform();
    Data.PlayerTransform = FTransform(FRotator(0.0f, Respawn->GetComponentRotation().Yaw, 0.0f), Respawn->GetComponentLocation());
    Data.EntranceDoorTag = CurrentRoomEntranceTag;
    Data.UnlockedDoorTags = UnlockedDoorTags;
    Data.CompletedPuzzleIds = CompletedPuzzleIds;
    Data.RoomsCompleted = RoomsCompleted;
    Data.LastRouteRoomClass = TSoftClassPtr<AMazeActors>(LastRouteRoomClass.Get());
    Data.ConsecutiveRouteRooms = ConsecutiveRouteRooms;
    Data.bValid = !Data.RoomTransform.ContainsNaN() && !Data.PlayerTransform.ContainsNaN();
    return Data.bValid;
}

bool AMazeGenerator::CanSaveAtCurrentStartRoom(APawn* Player) const
{
    if (!IsValid(CurrentRoom) || !IsValid(Player) || !Player->IsPlayerControlled() || GenerationStage != EGenerationStage::Idle || (LoopState != EMazeLoopState::WaitingForDoor && LoopState != EMazeLoopState::TraversingMaze))
    {
        return false;
    }

    TArray<UBoxComponent*> Boxes;
    CurrentRoom->GetComponents<UBoxComponent>(Boxes);

    for (UBoxComponent* Box : Boxes)
    {
        if (IsValid(Box) && (Box->ComponentHasTag(TEXT("RoomArea")) || Box->GetFName() == FName(TEXT("RoomArea")) || Box->ComponentHasTag(TEXT("RoomArrival"))) && Box->IsOverlappingActor(Player))
        {
            return true;
        }
    }

    return false;
}

// Loading starts with an empty layout. The normal door flow creates the next maze later.
bool AMazeGenerator::RestoreStartRoomState(const FMazeStartRoomSaveData& Data)
{
    UClass* RoomClass = Data.RoomClass.LoadSynchronous();

    if (!Data.bValid || !GetWorld() || !RoomClass || Data.RoomsCompleted < 0 || Data.RoomTransform.ContainsNaN() || Data.PlayerTransform.ContainsNaN() || Data.UnlockedDoorTags.IsEmpty())
    {
        return false;
    }

    for (const FName& DoorTag : Data.UnlockedDoorTags)
    {
        if (!DoorTags.Contains(DoorTag) || DoorTag == Data.EntranceDoorTag)
        {
            return false;
        }
    }

    if (!PuzzleRoomsById.IsEmpty())
    {
        TArray<FName> UniquePuzzles;

        for (const FName& PuzzleId : Data.CompletedPuzzleIds)
        {
            if (!PuzzleRoomsById.Contains(PuzzleId) || UniquePuzzles.Contains(PuzzleId))
            {
                return false;
            }

            UniquePuzzles.Add(PuzzleId);
        }

        if (UniquePuzzles.Num() != Data.RoomsCompleted || UniquePuzzles.Num() >= 3)
        {
            return false;
        }
    }

    AMazeActors* PreviousRoom = CurrentRoom;
    AMazeActors* RestoredRoom = nullptr;

    // Keep placed-instance settings when restoring the original start room.
    if (Data.RoomsCompleted == 0 && IsValid(InitialStartRoom) && InitialStartRoom->GetClass() == RoomClass)
    {
        RestoredRoom = InitialStartRoom;
    }
    else
    {
        FActorSpawnParameters Parameters;
        Parameters.Owner = this;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RestoredRoom = GetWorld()->SpawnActor<AMazeActors>(RoomClass, Data.RoomTransform, Parameters);
    }

    if (!IsValid(RestoredRoom) || !ValidateRoom(RestoredRoom, Data.EntranceDoorTag))
    {
        if (IsValid(RestoredRoom) && RestoredRoom != PreviousRoom)
        {
            RestoredRoom->Destroy();
        }

        return false;
    }

    ClearGeneratedLayout();
    MakeMovable(RestoredRoom);
    RestoredRoom->SetActorTransform(Data.RoomTransform);
    CurrentRoom = RestoredRoom;

    if (IsValid(PreviousRoom) && PreviousRoom != RestoredRoom)
    {
        if (InitialStartRoom == PreviousRoom)
        {
            InitialStartRoom = nullptr;
        }

        PreviousRoom->Destroy();
    }

    CurrentRoomEntranceTag = Data.EntranceDoorTag;
    UnlockedDoorTags = Data.UnlockedDoorTags;
    CompletedPuzzleIds = Data.CompletedPuzzleIds;
    RoomsCompleted = Data.RoomsCompleted;
    LastRouteRoomClass = Data.LastRouteRoomClass.LoadSynchronous();
    ConsecutiveRouteRooms = FMath::Clamp(Data.ConsecutiveRouteRooms, 0, 2);
    ActivePuzzleId = NAME_None;
    ActivePuzzleRoomClass = nullptr;
    bCurrentPuzzleCompleted = false;
    ChosenDoorTag = NAME_None;
    LoopState = EMazeLoopState::WaitingForDoor;
    InitializeRoom(CurrentRoom);
    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);
    return true;
}