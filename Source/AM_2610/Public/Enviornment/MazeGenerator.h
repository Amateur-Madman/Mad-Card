// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "MazeGenerator.generated.h"

class AMazeActors;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class APawn;
class ANavMeshBoundsVolume;
class UMazePuzzleComponent;
struct FMazeStartRoomSaveData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMazeGenerationFinished, AMazeActors*, Room, FName, DoorTag, bool, bSuccess);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazePuzzleCompleted, FName, PuzzleId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllMazePuzzlesEscaped);

UENUM(BlueprintType)
enum class EMazeLoopState : uint8
{
    WaitingForDoor,
    Generating,
    TraversingMaze,
    ChangingRooms,
    Completed
};

UCLASS()
class AM_2610_API AMazeGenerator : public AActor
{
    GENERATED_BODY()

public:
    AMazeGenerator();

    // Enemy hallway Blueprints. The same class can be placed more than once.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies")
    TArray<TSubclassOf<AMazeActors>> EnemyCorridorClasses;

    // First hallway of each type. Keeps the existing Blueprint lookup available.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Enemies")
    TMap<FName, TObjectPtr<AMazeActors>> SpawnedEnemyHallways;

    // All accepted enemy hallways, including repeated enemy types.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Enemies")
    TArray<TObjectPtr<AMazeActors>> SpawnedEnemyHallwayActors;

    // Counts for other hallway enemy tags. Chaser is always one; puzzle Angels are excluded.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies", meta = (ClampMin = "0", ClampMax = "16"))
    TMap<FName, int32> EnemyHallwayCounts;

    // Other enemy hallways can use the main route or a side branch.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies")
    bool bAllowEnemyHallwaysOnBranches = true;

    // Connected ordinary hallways required between any two enemy hallways. Rooms do not count.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies", meta = (ClampMin = "0", ClampMax = "10"))
    int32 MinHallwaysBetweenEnemyHallways = 2;

    // NavMeshBoundsVolume already placed in the level.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Maze|Navigation")
    TObjectPtr<ANavMeshBoundsVolume> MazeNavBounds = nullptr;

    // Multiplies the completed maze's world X width.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Navigation", meta = (ClampMin = "1.0"))
    float NavBoundsWidthMultiplier = 1.1f;

    // Multiplies the completed maze's world Y length.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Navigation", meta = (ClampMin = "1.0"))
    float NavBoundsLengthMultiplier = 1.1f;

    // Extra clearance on each side after applying the width and length multipliers.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Navigation", meta = (ClampMin = "0.0", Units = "cm"))
    float NavBoundsHorizontalPadding = 100.0f;

    // Extra clearance above and below the completed maze.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Navigation", meta = (ClampMin = "0.0", Units = "cm"))
    float NavBoundsHeightPadding = 200.0f;

    // Maximum time to wait for navigation before reporting generation failure.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Navigation", meta = (ClampMin = "1.0", Units = "s"))
    float NavigationBuildTimeout = 30.0f;

    // Measured world-space size of all accepted maze pieces.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Navigation")
    FVector GeneratedMazeSize = FVector::ZeroVector;

    // Final navigation volume size after multipliers and padding.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Navigation")
    FVector GeneratedNavBoundsSize = FVector::ZeroVector;

    // One safe player-capsule position inside both BP_Start and BP_Start_Next.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Saving")
    FName PlayerRespawnTag = TEXT("PlayerRespawn");

    // First start room already placed in the level.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Maze|Rooms")
    TObjectPtr<AMazeActors> InitialStartRoom = nullptr;

    // Component tag on BP_Exit.Connection_B.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    FName EndRoomExitTag = TEXT("ToNextStartRoom");

    // Duplicate start-room Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TSubclassOf<AMazeActors> NextStartRoomClass;

    // Component tag on BP_Start_Next.Connection_D.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    FName NextRoomEntranceDoorTag = TEXT("Door4");

    // Three outgoing choices.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TArray<FName> DoorTags = { FName(TEXT("Door1")), FName(TEXT("Door2")), FName(TEXT("Door3")) };

    // Exactly two outgoing doors unlock after arrival.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TArray<FName> NextRoomUnlockedDoorTags = { FName(TEXT("Door1")), FName(TEXT("Door2")) };

    // Entire player collision must clear the entrance before cleanup.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms", meta = (ClampMin = "0.0"))
    float ArrivalClearance = 25.0f;

    // Entire player collision must clear the chosen door before it locks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms", meta = (ClampMin = "0.0"))
    float DepartureClearance = 25.0f;

    // Corridor Blueprints available to the generator.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Pieces")
    TArray<TSubclassOf<AMazeActors>> CorridorClasses;

    // Optional end cap with one connection arrow. (Closes unfinished side openings.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Pieces")
    TSubclassOf<AMazeActors> EndCapClass;

    // Number of hallway pieces along the main route. (Includes turns.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxHallwaysForward = 15;

    // Rooms repeat along the main route in array order.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TArray<TSubclassOf<AMazeActors>> RouteRoomClasses;

    // Use random room types instead of repeating the array order.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    bool bRandomizeRouteRooms = true;

    // Zero allows a room at every interval until the route ends.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms", meta = (ClampMin = "0", ClampMax = "20"))
    int32 MaxRouteRooms = 15;

    // Enemy hallways count towards this spacing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms", meta = (ClampMin = "1", ClampMax = "10"))
    int32 HallwaysBetweenRooms = 2;

    // Side paths can place card and trap rooms using HallwaysBetweenRooms.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches")
    bool bAllowRoomsOnBranches = true;

    // Maximum rooms across all side paths. Zero removes this extra limit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches", meta = (ClampMin = "0", ClampMax = "10"))
    int32 MaxBranchRooms = 2;

    // Allow T-cross and four-way pieces along the main route.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches")
    bool bAllowJunctions = true;

    // Minimum preferred spacing between junctions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MinHallwaysBetweenJunctions = 2;

    // Maximum preferred spacing between junctions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxHallwaysBetweenJunctions = 4;

    // Maximum number of extra hallway pieces across all side branches.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Branches", meta = (ClampMin = "0", ClampMax = "10"))
    int32 MaxBranchHallways = 8;

    // Number of layout attempts if the main route cannot be completed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "1", ClampMax = "20"))
    int32 LayoutAttempts = 20;

    // Maximum candidate actors spawned during one generation frame.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Performance", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxCandidateSpawnsPerFrame = 20;

    // Stop starting more generation work after this frame budget is used.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Performance", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float GenerationTimeBudgetMs = 2.0f;

    // Initializes repeatable random choices. Same inputs and seed repeat the layout.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout")
    int32 RandomSeed = 0;

    // Allowed error when matching connection arrow positions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "0.1"))
    float ConnectionTolerance = 5.0f;

    // Allowed overlap at seams between MazeBounds boxes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "0.0"))
    float BoundsSeamTolerance = 2.0f;

    // Check ahead of an opening before growing towards another maze piece. Zero disables the radius check.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Layout", meta = (ClampMin = "0.0", ClampMax = "2000.0", Units = "cm"))
    float ConnectionClearanceRadius = 300.0f;

    // Show unfinished side openings with red debug arrows.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Debug")
    bool bDrawOpenConnections = true;

    // Show a button at every puzzle spawn arrow to check their positions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Debug")
    bool bSpawnAllPuzzleButtons = false;

    // Test every puzzle enemy arrow. Blocked positions fail and are logged.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Debug")
    bool bSpawnAllPuzzleWeepingAngels = false;

    // True when the requested main route is complete. (Does not guarantee branch loops.)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Result")
    bool bMainRouteComplete = false;

    // Connection position and rotation for the future start room.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Result")
    FTransform StartConnectionTransform;

    // Connection position and rotation for the future end room.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Result")
    FTransform EndConnectionTransform;

    // Number of side openings that have not been connected or capped.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Result")
    int32 UnfinishedBranchOpenings = 0;

    // Start room spawned before the maze.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TSubclassOf<AMazeActors> StartRoomClass;

    // End room attached to the last hallway.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TSubclassOf<AMazeActors> EndRoomClass;

    // Three different puzzle rooms, each with a stable ID for completed-puzzle tracking.
    // Leave empty to use the existing maze flow without puzzle progression.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles")
    TMap<FName, TSubclassOf<AMazeActors>> PuzzleRoomsById;

    // The puzzle room's exit connects to hallways before BP_Exit and BP_Start_Next.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles")
    FName PuzzleRoomExitTag = TEXT("ToMazeExit");

    // Choose a random puzzle position while leaving main-route hallways on both sides.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MinHallwaysBeforePuzzle = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MinHallwaysAfterPuzzle = 6;

    // Add extra hallways at the end of the route to keep space before the exit/start-room pair.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles", meta = (ClampMin = "1", ClampMax = "20"))
    int32 HallwaysAfterPuzzle = 3;

    // Puzzle-only enemy type. Keep its pawn Blueprint assigned in EnemyClassesByTag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles")
    FName WeepingAngelEnemyTag = TEXT("EnemyWeepingAngel");

    // BP_Exit forwards this tag to its actual end-door Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Puzzles")
    FName MazeEndDoorTag = TEXT("EndDoor");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Puzzles")
    TArray<FName> CompletedPuzzleIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Puzzles")
    FName ActivePuzzleId = NAME_None;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Puzzles")
    FOnMazePuzzleCompleted OnPuzzleCompleted;

    // Fired after solving and escaping the third maze. Bind the win screen here.
    UPROPERTY(BlueprintAssignable, Category = "Maze|Puzzles")
    FOnAllMazePuzzlesEscaped OnAllPuzzlesEscaped;

    // Set this to Door1, Door2, or Door3.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    FName StartDoorTag = TEXT("Door1");

    // Class of player spawned inside the start room.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
    TSubclassOf<APawn> PlayerClass;

    // Spawn arrows use these tags to select the enemy pawn Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies")
    TMap<FName, TSubclassOf<APawn>> EnemyClassesByTag;

    // Match the enemy type tag used by the Chaser's spawn arrow.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies")
    FName ChaserEnemyTag = FName(TEXT("Chaser"));

    // Box component tag in the Chaser hallway. Entering it starts the countdown once.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies")
    FName ChaserSpawnTriggerTag = TEXT("ChaserSpawnTrigger");

    // Wait this long after the player enters the tagged box before spawning the Chaser.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Enemies", meta = (ClampMin = "0.0", Units = "s"))
    float ChaserSpawnDelay = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    EMazeLoopState LoopState = EMazeLoopState::WaitingForDoor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    TObjectPtr<AMazeActors> CurrentRoom = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    TObjectPtr<AMazeActors> NextRoom = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    TArray<FName> UnlockedDoorTags;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    FName CurrentRoomEntranceTag = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    FName ChosenDoorTag = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    FName NextRoomEntranceTag = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Maze|Loop")
    int32 RoomsCompleted = 0;

    // Fired after generation succeeds or all layout attempts fail.
    UPROPERTY(BlueprintAssignable, Category = "Maze|Loop")
    FOnMazeGenerationFinished OnMazeGenerationFinished;

    // True means the door can open now. A new request starts generation and returns false.
    // Bind OnMazeGenerationFinished before calling this for a new door choice.
    UFUNCTION(BlueprintCallable, Category = "Maze|Doors")
    bool ChooseDoor(AMazeActors* Room, FName DoorTag);

    // Rebuild the chosen route for testing. True means the request was queued.
    UFUNCTION(BlueprintCallable, Category = "Maze|Debug")
    bool RebuildMazeAtChosenDoor();

    UFUNCTION(BlueprintPure, Category = "Maze|Doors")
    bool IsDoorUnlocked(AMazeActors* Room, FName DoorTag) const;

    // Future puzzles can unlock another outgoing door.
    UFUNCTION(BlueprintCallable, Category = "Maze|Doors")
    bool SetDoorUnlocked(AMazeActors* Room, FName DoorTag, bool bUnlocked);

    // Called after the player has fully entered the next start room.
    UFUNCTION(BlueprintCallable, Category = "Maze|Loop")
    bool PlayerEnteredNextRoom(AMazeActors* Room, APawn* Player);

    // Called when the chosen start door finishes closing behind the player.
    UFUNCTION(BlueprintCallable, Category = "Maze|Loop")
    bool PlayerEnteredMaze(AMazeActors* Room, FName DoorTag, APawn* Player);

    // Save/restore only the current start room. Generating a maze still requires a door choice.
    bool CaptureStartRoomState(FMazeStartRoomSaveData& Data) const;
    bool RestoreStartRoomState(const FMazeStartRoomSaveData& Data);
    bool CanSaveAtCurrentStartRoom(APawn* Player) const;

    // The puzzle component calls this after its actual puzzle is solved.
    bool RegisterPuzzleCompleted(AMazeActors* Room);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class EGenerationStage : uint8
    {
        Idle,
        Cleanup,
        StartAttempt,
        MainRoute,
        PuzzleRoom,
        ExitHallways,
        EndRoom,
        NextRoom,
        Branches,
        Caps,
        PuzzleButtons,
        NavigationBounds,
        Navigation,
        Enemies,
        PuzzleEnemies,
        Complete
    };

    enum class EAttachmentResult : uint8
    {
        Pending,
        Succeeded,
        Failed
    };

    // Each side opening keeps its own spacing and previous room choices.
    struct FBranchRoomState
    {
        int32 HallwaysSinceRoom = 0;
        TWeakObjectPtr<UClass> LastRoomClass;
        int32 ConsecutiveRooms = 0;
    };

    TMap<TWeakObjectPtr<UArrowComponent>, FBranchRoomState> BranchRoomStates;
    FBranchRoomState GenerationBranchRoomState;
    int32 GenerationBranchRoomCount = 0;
    bool bTryingBranchRoom = false;
    bool bCappingBranch = false;

    // Cache the end cap's footprint once so future openings keep enough space to close.
    TArray<FVector> EndCapFootprintCorners;
    FVector EndCapEntranceLocation = FVector::ZeroVector;
    FVector EndCapEntranceForward = FVector::ForwardVector;
    FVector EndCapScale = FVector::OneVector;

    EGenerationStage GenerationStage = EGenerationStage::Idle;

    UPROPERTY(Transient)
    TObjectPtr<AMazeActors> SpawnedPuzzleRoom = nullptr;

    UPROPERTY(Transient)
    TSubclassOf<AMazeActors> ActivePuzzleRoomClass;

    bool bCurrentPuzzleCompleted = false;
    bool bPuzzlePrepared = false;
    bool bRebuildingPuzzle = false;

    bool SelectPuzzleRoom();
    TArray<FName> GetRemainingPuzzleIds() const;
    UMazePuzzleComponent* GetPuzzleComponent() const;

    // Saved work allows generation to continue on the following frame.
    int32 GenerationSerial = 0;
    int32 GenerationAttempt = 0;
    int32 GenerationAttemptLimit = 0;
    int32 GenerationHallwayIndex = 0;
    int32 GenerationHallwayCount = 0;
    // Insert the puzzle after this many main-route hallways without using a hallway slot.
    int32 GenerationPuzzleHallwayIndex = 0;
    int32 GenerationExitHallwayCount = 0;
    int32 GenerationExitHallwayIndex = 0;
    int32 GenerationNextJunctionAt = 0;
    int32 GenerationMinJunctionSpacing = 0;
    int32 GenerationMaxJunctionSpacing = 0;
    int32 GenerationBranchCount = 0;
    int32 GenerationBranchLimit = 0;
    int32 GenerationCapIndex = -1;
    int32 CandidateSpawnsThisFrame = 0;

    bool bRetryAfterCleanup = false;
    bool bTryingMainJunction = false;
    bool bAttachmentPending = false;

    TArray<FName> PreviousUnlockedDoorTags;
    TArray<TWeakObjectPtr<UArrowComponent>> BlockedConnections;
    TWeakObjectPtr<UArrowComponent> GenerationPreviousExit;
    TWeakObjectPtr<UArrowComponent> GenerationTarget;

    UPROPERTY(Transient)
    TArray<TSubclassOf<AMazeActors>> PendingAttachmentClasses;

    int32 PendingAttachmentClassIndex = 0;
    int32 PendingMinConnections = 0;
    int32 PendingMaxConnections = 0;
    FVector PendingTargetPosition = FVector::ZeroVector;
    FVector PendingTargetForward = FVector::ForwardVector;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AMazeActors>> SpawnedMazeActors;

    UPROPERTY(Transient)
    TObjectPtr<AMazeActors> SpawnedEndRoom = nullptr;

    // Matches SpawnedEnemyHallwayActors by index. Null means this spawn is pending.
    UPROPERTY(Transient)
    TArray<TObjectPtr<APawn>> SpawnedEnemies;

    // Keep the history from before this layout when rebuilding it for testing.
    UPROPERTY(Transient)
    TSubclassOf<AMazeActors> RouteRoomClassBeforeMaze;

    int32 ConsecutiveRouteRoomsBeforeMaze = 0;

    // Track accepted rooms so one type cannot appear three times in a row.
    UPROPERTY(Transient)
    TSubclassOf<AMazeActors> LastRouteRoomClass;
    int32 ConsecutiveRouteRooms = 0;

    // Failed attempts must not change the saved room history.
    UPROPERTY(Transient)
    TSubclassOf<AMazeActors> GenerationLastRouteRoomClass;

    int32 GenerationConsecutiveRouteRooms = 0;

    TArray<TWeakObjectPtr<UArrowComponent>> OpenConnections;
    TWeakObjectPtr<UArrowComponent> StartArrow;
    TWeakObjectPtr<UArrowComponent> EndArrow;

    TWeakObjectPtr<UBoxComponent> PendingArrivalTrigger;
    TWeakObjectPtr<APawn> PendingArrivalPlayer;

    FTimerHandle ArrivalCheckTimer;
    FTimerHandle ChaserSpawnTimer;

    TWeakObjectPtr<UBoxComponent> ChaserSpawnTrigger;
    int32 ChaserSpawnHallwayIndex = INDEX_NONE;
    bool bChaserCountdownStarted = false;

    bool PrepareChaserSpawnTrigger(int32 HallwayIndex);
    void EnableChaserSpawnTrigger();
    void ClearChaserSpawnTrigger();

    UFUNCTION()
    void OnChaserSpawnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    FRandomStream RandomStream;

    bool ValidateRoom(AMazeActors* Room, FName IncomingDoorTag = NAME_None) const;
    void InitializeRoom(AMazeActors* Room);
    void ApplyDoorLocks(AMazeActors* Room, const TArray<FName>& AllowedDoors);

    UArrowComponent* FindTaggedConnection(AMazeActors* MazeActor, const FName& RequiredTag) const;
    UBoxComponent* FindArrivalTrigger(AMazeActors* Room) const;

    UFUNCTION()
    void OnRoomArrivalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnRoomArrivalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

    void CheckPendingArrival();
    void ClearPendingArrival();
    bool HasPlayerClearedEntrance(APawn* Player) const;

    bool GenerateMaze();
    void ClearGeneratedLayout();
    void StepGeneration();
    void StepMainRoute();
    void StepBranches();
    void StepCaps();
    void BeginGenerationCleanup(bool bRetry);
    void FinishGeneration(bool bSuccess);
    void CancelGeneration();
    bool SpawnEndRoom();
    bool SpawnNextRoom();
    bool PrepareEnemyTypes();
    bool PrepareEnemyHallways();
    FName ChooseEnemyHallwayType(UArrowComponent* Opening, int32 RemainingHallways);
    bool RegisterEnemyHallway(AMazeActors* Piece, FName EnemyType);
    void TrackMazeConnections(AMazeActors* Piece, bool bHallway);
    void GetHallwayDistances(AMazeActors* From, TMap<TWeakObjectPtr<AMazeActors>, int32>& Distances) const;
    bool ValidateEnemyHallwaySpacing() const;
    bool SpawnEnemyInHallway(int32 HallwayIndex, bool bSpawnImmediately = false);
    void SpawnDelayedChaser(int32 HallwayIndex);
    UArrowComponent* FindEnemySpawnPoint(AMazeActors* Piece) const;
    FName FindEnemyTypeForSpawnPoint(UArrowComponent* SpawnPoint) const;
    TSubclassOf<APawn> FindEnemyClassForSpawnPoint(UArrowComponent* SpawnPoint) const;
    bool IsEnemyCorridorClass(TSubclassOf<AMazeActors> PieceClass) const;
    bool UpdateMazeNavigationBounds();
    void StepNavigation();
    int32 GenerationRouteRoomCount = 0;
    int32 GenerationHallwaysSinceRoom = 0;
    bool IsRouteRoomClass(TSubclassOf<AMazeActors> PieceClass) const;
    bool ShouldPlaceRouteRoom() const;
    bool ShouldPlaceBranchRoom(const FBranchRoomState& State) const;

    void StartAttachment(const FVector& TargetPosition, const FVector& TargetForward, int32 MinConnections, int32 MaxConnections, bool bCap, FName EnemyType = NAME_None, bool bRouteRoom = false, TSubclassOf<AMazeActors> ForcedClass = nullptr);
    EAttachmentResult StepAttachment(AMazeActors*& Piece, UArrowComponent*& Entrance);

    bool GetFootprints(AMazeActors* Piece, TArray<FBox>& Boxes) const;
    bool OverlapsMaze(AMazeActors* Piece, UArrowComponent* IgnoredOpening = nullptr) const;
    bool FootprintsOverlap(const TArray<FBox>& A, const TArray<FBox>& B) const;
    bool IsBoundsSeamAllowed(AMazeActors* First, AMazeActors* Second, const FBox& Overlap) const;
    bool PrepareEndCapFootprint();
    bool GetEndCapFootprints(UArrowComponent* Connection, TArray<FBox>& Boxes) const;
    void GetReservedCapFootprints(TArray<FBox>& Boxes, UArrowComponent* IgnoredOpening = nullptr) const;
    bool CanReserveEndCaps(AMazeActors* Piece, UArrowComponent* Entrance, UArrowComponent* ReservedExit) const;
    bool IsConnectionCrowded(UArrowComponent* Connection) const;
    TArray<UArrowComponent*> GetRouteExits(AMazeActors* Piece, UArrowComponent* Entrance) const;
    bool ConnectionsMatch(UArrowComponent* A, UArrowComponent* B) const;
    void JoinMatchingOpenConnections();
    void AddUnusedConnections(AMazeActors* Piece, UArrowComponent* Entrance, UArrowComponent* ReservedExit);

    // Main-route and branch placements share the same remaining enemy counts.
    TArray<FName> GenerationEnemyTypes;
    TArray<FName> GenerationRemainingEnemyTypes;

    // Real connections keep spacing correct across junctions and joined branches.
    TMap<TWeakObjectPtr<AMazeActors>, TArray<TWeakObjectPtr<AMazeActors>>> GenerationPieceLinks;
    TSet<TWeakObjectPtr<AMazeActors>> GenerationHallwayPieces;
    // Enemy type required by the current attachment.
    FName PendingEnemyType = NAME_None;
    // Continue enemy spawning across frames within the generation budget.
    int32 GenerationEnemySpawnIndex = 0;
    // Start time for the navigation build timeout.
    double NavigationWaitStartedAt = 0.0;
};