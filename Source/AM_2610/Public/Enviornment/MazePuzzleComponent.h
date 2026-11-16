// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Enviornment/MazePipePuzzle.h"
#include "MazePuzzleComponent.generated.h"

class AMazeGenerator;
class AMazePuzzleButton;
class AMazePipePiece;
class APawn;
class UArrowComponent;
class UBoxComponent;
class ULineBatchComponent;
class ULightComponent;
class UPrimitiveComponent;
class UMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPuzzleButtonsUnlocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleConsoleStateChanged, bool, bRed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPuzzleConsoleActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomPuzzleCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPipeSteamCheckFinished, bool, bPassed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPipeTrapTriggered);

// Add this component to a room without changing its BP_RoomBase parent.
UCLASS(ClassGroup = (Maze), meta = (BlueprintSpawnableComponent))
class AM_2610_API UMazePuzzleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMazePuzzleComponent();

    // Disable this for the other puzzles if they use different entry conditions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    bool bUsePressureButtons = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    TSubclassOf<AMazePuzzleButton> ButtonClass;

    // Two different tagged arrows are selected for the two buttons.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    FName ButtonSpawnTag = TEXT("PuzzleButtonSpawn");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Buttons")
    FName PuzzleDoorTag = TEXT("PuzzleDoor");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    bool bUsePipePuzzle = false;

    // Test a placed board without a generator. This never completes a maze room or opens its door.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Puzzle|Pipes|Testing")
    bool bAllowStandalonePipeTesting = false;

    // Square cell size and fitted columns are calculated from the arrow rectangle.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FMazePipeGridSettings PipeGridSettings;

    // Space reserved between the generated pipes and the rectangle edges.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes", meta = (ClampMin = "0.0", Units = "cm"))
    float PipeBorderPadding = 0.0f;

    // Distance from the rectangle plane toward the inside of the room.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes", meta = (ClampMin = "0.0", Units = "cm"))
    float PipeWallOffset = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes")
    TSubclassOf<AMazePipePiece> StraightPipeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes")
    TSubclassOf<AMazePipePiece> BendPipeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes")
    TSubclassOf<AMazePipePiece> TeePipeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes")
    TSubclassOf<AMazePipePiece> ChamberPipeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes")
    TSubclassOf<AMazePipePiece> CrossPipeClass;

    // Diameter as a fraction of square cell size. Shared by all piece types.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes", meta = (ClampMin = "0.02", ClampMax = "0.35"))
    float PipeDiameterFraction = 0.12f;

    // Extra distance that the fixed inlet and outlet tails extend into the wall.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Meshes", meta = (ClampMin = "0.0", Units = "cm"))
    float PipeWallEmbedDepth = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName LeftBottomLeftTag = TEXT("LeftBottomLeft");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName LeftBottomRightTag = TEXT("LeftBottomRight");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName LeftTopLeftTag = TEXT("LeftTopLeft");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName LeftTopRightTag = TEXT("LeftTopRight");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName RightBottomLeftTag = TEXT("RightBottomLeft");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName RightBottomRightTag = TEXT("RightBottomRight");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName RightTopLeftTag = TEXT("RightTopLeft");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes")
    FName RightTopRightTag = TEXT("RightTopRight");

    // Only used by the editor preview buttons. Runtime uses the supplied puzzle seed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes", meta = (DisplayName = "Pipe Preview Seed"))
    int32 PipePreviewSeed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes", meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "s"))
    float PipePreviewDuration = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Lights", meta = (ClampMin = "0.02", ClampMax = "2.0", Units = "s"))
    float PipeSteamStepSeconds = 0.2f;

    // Failed checks keep their temporary colors for this duration. Completed walls stay green.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Lights", meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "s"))
    float PipeSteamHoldSeconds = 4.0f;

    // Tag a tally mesh or its Child Actor Component. Each resolved mesh counts once.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    FName PipeFailureTallyTag = TEXT("PipeFailureTally");

    // Marks are ordered along their row in room coordinates. Reverse if the first end is wrong.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    bool bReversePipeFailureTallies = false;

    // Only the material slot for the glowing mark is replaced.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies", meta = (ClampMin = "0"))
    int32 PipeFailureTallyMaterialIndex = 0;

    // Optional shared source material. Empty uses each tagged mesh's current material.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    TObjectPtr<UMaterialInterface> PipeFailureTallyMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    FName PipeFailureTallyColorParameter = TEXT("TallyColor");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    FLinearColor PipeFailureTallyOffColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Tallies")
    FLinearColor PipeFailureTallyFailedColor = FLinearColor(3.0f, 0.0f, 0.0f);

    // One cue per recorded failed test, including the final failure that starts the trap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback")
    TObjectPtr<USoundBase> PipeWrongChoiceSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float PipeWrongChoiceVolume = 1.0f;

    // Assign NS_Thruster_SmokeTrail. Each traced leak gets its own temporary component.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback")
    TObjectPtr<UNiagaraSystem> PipeLeakSteamSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.1", ClampMax = "30.0", Units = "s"))
    float PipeLeakSteamDuration = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float PipeLeakSteamScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", Units = "cm"))
    float PipeLeakSteamOutletOffset = 1.0f;

    // Keep rising smoke from a downward opening in front of the pipe surface.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback",
        meta = (ClampMin = "0.0", ClampMax = "20.0", Units = "cm", DisplayName = "Downward Leak Front Clearance",
            ToolTip = "Extra room facing clearance beyond the pipe radius for downward leaks. This keeps rising smoke in front of the pipe."))
    float PipeLeakSteamDownwardFrontClearance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback")
    FLinearColor PipeLeakSteamColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float PipeLeakSteamDensity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
    float PipeLeakSteamSpeedMin = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
    float PipeLeakSteamSpeedMax = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
    float PipeLeakSteamDirectionStrength = 100.0f;

    // Use local X when the Niagara asset interprets TrailVector in emitter space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Pipes|Failure Feedback",
        meta = (DisplayName = "Pipe Leak Steam Vector Is Local", ToolTip = "Use local X for TrailVector. The spawned component rotation already aims out of the pipe opening."))
    bool bPipeLeakSteamVectorIsLocal = false;

    // Five tagged assets give five failures; three tagged assets give three failures.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    int32 PipeFailureTallyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    bool bPipeTrapTriggered = false;

    // Connect this once to the room's trap sequence. The final tally is red before it fires.
    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Pipes|Events")
    FOnPipeTrapTriggered OnPipeTrapTriggered;

    // One shared count per room. Both failed Hard walls still consume one tally per test.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    int32 PipeFailedSteamChecks = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    bool bPipeSteamChecking = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    FMazePipeSteamResult LeftPipeSteamResult;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    FMazePipeSteamResult RightPipeSteamResult;

    // Fired once after the steam wave reaches every currently connected branch.
    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Pipes|Events")
    FOnPipeSteamCheckFinished OnPipeSteamCheckFinished;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    bool bLeftPipeActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    bool bRightPipeActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    bool bLeftPipeCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, DuplicateTransient, Category = "Puzzle|Pipes|State")
    bool bRightPipeCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    FMazePipeLayout LeftPipeLayout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|Pipes|State")
    FMazePipeLayout RightPipeLayout;

    // Tag only the doorway indicator meshes, including meshes inside child actors.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Door Status")
    FName StatusLightComponentTag = TEXT("PuzzleStatusLight");

    // Material slot containing the glowing strips, rather than the door/frame body.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Door Status", meta = (ClampMin = "0"))
    int32 StatusLightMaterialIndex = 0;

    // Vector parameter in the indicator material, multiplied into Emissive Color.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Door Status")
    FName StatusLightColorParameter = TEXT("StatusColor");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Door Status")
    FLinearColor IncompleteLightColor = FLinearColor(1.0f, 0.02f, 0.02f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Door Status")
    FLinearColor CompletedLightColor = FLinearColor(0.02f, 1.0f, 0.05f);

    // Existing whole-room overlap box. Keep it around the room and through the doorway.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Departure")
    FName CompletedExitAreaTag = TEXT("RoomArea");

    // The enemy class comes from the generator's Weeping Angel type tag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Enemies", meta = (ClampMin = "0", ClampMax = "16"))
    int32 WeepingAngelCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Enemies")
    FName EnemySpawnTag = TEXT("PuzzleEnemySpawn");

    // Separate entry box. Angels wait until the player overlaps this component tag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Enemies")
    FName EnemySpawnTriggerTag = TEXT("PuzzleEnemySpawnTrigger");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    FName PuzzleId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleStarted = false;

    // Both buttons make the console ready. They do not unlock the door directly.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bButtonsUnlocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleDoorUnlocked = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bConsoleRed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    bool bPuzzleCompleted = false;

    // Remains sealed after a completed puzzle's player has fully left its room area.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Puzzle|State")
    bool bPuzzleDoorSealed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    TArray<TObjectPtr<AMazePuzzleButton>> SpawnedButtons;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|State")
    TArray<TObjectPtr<APawn>> SpawnedWeepingAngels;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleButtonsUnlocked OnButtonsUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleConsoleStateChanged OnConsoleStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnPuzzleConsoleActivated OnConsoleActivated;

    UPROPERTY(BlueprintAssignable, Category = "Puzzle|Events")
    FOnRoomPuzzleCompleted OnPuzzleCompleted;

    // Call after the player has fully entered a pressure button or pipe room.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool BeginPuzzle(APawn* Player);

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    bool CanActivateConsole(APawn* Player) const;

    // Call from the console's existing player interaction event.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool ActivateConsole(APawn* Player);

    // Record actual success. Pipe checks also unlock the room door and notify its opening graph.
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CompletePuzzle();

    // Select the existing save difficulty. Easy chooses one wall and Hard uses both.
    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    bool GeneratePipeLayouts(int32 Seed);

    UFUNCTION(BlueprintPure, Category = "Puzzle|Pipes")
    bool CanRotatePipeCell(bool bRightWall, int32 CellIndex) const;

    // Called by the pipe Actor through the player's existing interaction actions.
    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    bool RotatePipeCell(bool bRightWall, int32 CellIndex, bool bRotateRight);

    // Returns data only. The console's animated check handles pipe room completion.
    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    FMazePipeSteamResult TracePipeWall(bool bRightWall) const;

    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    bool GetPipeCellLocation(bool bRightWall, int32 CellIndex, FVector& Location, float& CellSize) const;

    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    bool BuildPipeMeshes();

    // Test current connections and animate their BFS distances.
    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes")
    bool CheckPipePuzzle();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Pipe Steam Check"))
    void PreviewPipeSteamCheck();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Reset Pipe Lights"))
    void ResetPipeLights();

    // Select the component on a placed room instance to use these preview buttons.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Pipe Grid"))
    void PreviewPipeGrid();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Pipe Layout"))
    void PreviewPipeLayout();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Pipe Solution"))
    void PreviewPipeSolution();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Failed Pipe Mesh"))
    void PreviewFailedPipeMesh();

    // Keep existing Blueprint calls valid while replacing the editor button.
    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes", meta = (DeprecatedFunction, DeprecationMessage = "Use PreviewFailedPipeMesh."))
    void PreviewPipeMeshFit();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Pipe Meshes"))
    void PreviewPipeMeshes();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Preview Solved Pipe Meshes"))
    void PreviewSolvedPipeMeshes();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipe Preview", meta = (DisplayName = "Clear Preview"))
    void ClearPreview();

    UFUNCTION(BlueprintCallable, Category = "Puzzle|Pipes", meta = (DeprecatedFunction, DeprecationMessage = "Use ClearPreview."))
    void ClearPipeMeshPreview();

    bool PreparePuzzle(AMazeGenerator* Generator, FName SelectedId, TSubclassOf<APawn> AngelClass, int32 Seed, bool bSpawnAllButtons = false, bool bSpawnAllEnemies = false);
    bool StepButtonSpawning(bool& bFinished);
    bool StepEnemySpawning(bool& bFinished);
    void ArmPuzzle();

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    bool BuildPipeLayouts(int32 Seed, bool bHard);
    bool BuildPipeFrame(bool bRightWall, FMazePipeGridFrame& OutFrame, FString& OutError) const;
    void DrawPipePreview(bool bShowLayout, bool bShowSolution);
    ULineBatchComponent* GetPipePreviewLines();
    void ClearPipePreviewObjects();
    void ResetPipeCompletion();
    TSubclassOf<AMazePipePiece> GetPipeMeshClass(EMazePipePiece Piece) const;
    USceneComponent* GetPipeWallParent(bool bRightWall) const;
    bool SpawnPipePreviewPiece(bool bRightWall, const FMazePipeGridFrame& Frame, int32 CellIndex, const FMazePipeCell& Cell);
    bool SpawnPipePiece(bool bRightWall, const FMazePipeGridFrame& Frame, int32 CellIndex, const FMazePipeCell& Cell, bool bPreview);
    void BuildPipeMeshPreview(bool bShowSolution, bool bForceFailure = false);
    void WarnPipeLightMaterials() const;
    void ClearRuntimePipeMeshes();
    void AdvancePipeSteamCheck(float DeltaTime);
    void ApplyPipeSteamStep();
    bool ArePipeWallsCompleted() const;
    bool IsStandalonePipeTest() const;
    void RecordPipeSteamFailure();
    void PlayPipeFailureFeedback();
    void SpawnPipeSteamLeaks(bool bRightWall, const FMazePipeSteamResult& Result);
    bool GetPipeLeakOutlet(bool bRightWall, const FMazePipeLeak& Leak, FVector& Location, FVector& Direction) const;
    void ClearPipeLeakSteam();
    bool InitializePipeFailureTallies();
    void UpdatePipeFailureTallies();
    void ClearPipeFailureTallies();

    UPROPERTY(Transient, DuplicateTransient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PipeFailureTallyMaterials;

    UPROPERTY(Transient, DuplicateTransient)
    TArray<TObjectPtr<UMaterialInterface>> PipeFailureTallyOriginalMaterials;

    TArray<TWeakObjectPtr<UMeshComponent>> PipeFailureTallyMeshes;

    struct FPipeTallyLightState
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        TWeakObjectPtr<ULightComponent> Light;
        FLinearColor OriginalColor;
        bool bOriginalVisible = false;
    };

    TArray<FPipeTallyLightState> PipeFailureTallyLights;

    UPROPERTY(Transient, DuplicateTransient)
    TArray<TObjectPtr<UNiagaraComponent>> PipeLeakSteamComponents;

    FTimerHandle PipeLeakSteamTimer;
    int32 CachedPipeFailureTallyMaterialIndex = INDEX_NONE;
    FName CachedPipeFailureTallyColorParameter = NAME_None;
    bool bPipeFailureRecorded = false;

    UPROPERTY(Transient, DuplicateTransient)
    TObjectPtr<ULineBatchComponent> PipePreviewLines;

    UPROPERTY(Transient, DuplicateTransient)
    TArray<TObjectPtr<AMazePipePiece>> PipeMeshActors;

    FMazePipeLayout CheckedLeftPipeLayout;
    FMazePipeLayout CheckedRightPipeLayout;
    bool bHasPipeLayoutPreview = false;
    bool bPipePreviewShowsSolution = false;
    bool bPipeCheckUsesPreview = false;
    bool bPipeCheckIsStandalone = false;
    bool bPipeSteamResultShown = false;
    int32 PipeSteamStep = 0;
    int32 LastPipeSteamStep = 0;
    float PipeSteamElapsed = 0.0f;
    float PipeSteamHoldRemaining = 0.0f;

    UPROPERTY(Transient, DuplicateTransient)
    TArray<TObjectPtr<AMazePipePiece>> PipeMeshPreviewActors;

    TWeakObjectPtr<AMazeGenerator> MazeGenerator;

    UPROPERTY(Transient)
    TSubclassOf<APawn> WeepingAngelClass;

    // Extra debug buttons are previews. Only SpawnedButtons are used by the puzzle.
    UPROPERTY(Transient)
    TArray<TObjectPtr<AMazePuzzleButton>> DebugButtons;

    TArray<TWeakObjectPtr<UArrowComponent>> ButtonSpawns;
    TArray<TWeakObjectPtr<UArrowComponent>> EnemySpawns;
    int32 ButtonSpawnIndex = 0;
    int32 EnemySpawnIndex = 0;
    int32 RequestedEnemyCount = 0;
    int32 RequestedButtonCount = 2;
    bool bDebugSpawnAllButtons = false;
    bool bDebugSpawnAllEnemies = false;
    bool bPrepared = false;
    bool bArmed = false;

    TWeakObjectPtr<UBoxComponent> EnemyEntryTrigger;
    bool bWaitingForEnemyEntry = false;
    bool bEnemySpawningPending = false;
    float EnemySpawnRetryRemaining = 0.0f;

    bool PrepareEnemyEntryTrigger();
    void EnableEnemyEntryTrigger();
    void ClearEnemyEntryTrigger();
    void AdvanceEntryEnemySpawning(float DeltaTime);

    UFUNCTION()
    void OnEnemyEntryTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    bool CanUnlockButtons(const TArray<APawn*>& First, const TArray<APawn*>& Second) const;
    bool IsEligibleOccupant(APawn* Pawn) const;
    void ClearSpawnedActors();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> StatusLightMaterials;

    TArray<TWeakObjectPtr<UMeshComponent>> StatusLightMeshes;
    int32 CachedStatusMaterialIndex = INDEX_NONE;
    void InitializeStatusLights();
    void UpdateStatusLights();

    TWeakObjectPtr<UBoxComponent> CompletedExitArea;
    TWeakObjectPtr<APawn> CompletedExitPlayer;
    bool bObservedPlayerInPuzzleArea = false;
    FTimerHandle CompletedExitTimer;
    void PrepareCompletedExitArea();
    void ClearCompletedExitArea();
    void QueueCompletedExitCheck();
    void CheckCompletedPuzzleExit();

    UFUNCTION()
    void OnCompletionAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnCompletionAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
};