#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "MazePipeTrap.generated.h"

class AActor;
class APawn;
class APlayerController;
class UBoxComponent;
class UPawnMovementComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
struct FMazePipeGridFrame;

UENUM(BlueprintType)
enum class EMazePipeTrapPhase : uint8
{
    Idle,
    Flipping,
    Closing,
    Closed,
    Revealing
};

UENUM(BlueprintType)
enum class EMazePipePanelFlipAxis : uint8
{
    Auto UMETA(DisplayName = "Automatic (smaller sweep)"),
    Up UMETA(DisplayName = "Vertical axis"),
    Right UMETA(DisplayName = "Horizontal axis")
};

// These settings appear on the existing MazePuzzleComponent. No new Actor or
// ActorComponent Blueprint is needed: FMazePipeTrap below is a plain C++ helper.
USTRUCT(BlueprintType)
struct AM_2610_API FMazePipeTrapSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap")
    FName LeftWallRootTag = TEXT("LeftWallRoot");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap")
    FName RightWallRootTag = TEXT("RightWallRoot");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid")
    bool bGenerateSpikesFromGrid = true;

    // Give OutsideL1..L4 and OutsideR1..R4 this shared Component Tag.
    // Inside wall meshes must not use this tag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid")
    FName OutsideWallTag = TEXT("PipeTrapOutsideWall");

    // InsideL1..L4 / InsideR1..R4 names are recognized automatically. This tag
    // can select inside panels with other names for spatial pairing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Panels")
    FName InsideWallTag = TEXT("PipeTrapInsideWall");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Panels")
    EMazePipePanelFlipAxis PanelFlipAxis = EMazePipePanelFlipAxis::Auto;

    // Empty uses the engine cone supplied by MazePuzzleComponent. The tip is local +Z.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid")
    TObjectPtr<UStaticMesh> SpikeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid")
    TObjectPtr<UMaterialInterface> SpikeMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid", meta = (ClampMin = "1.0", ClampMax = "500.0", Units = "cm"))
    float SpikeLength = 50.0f;

    // The cone base diameter is this fraction of the fitted pipe cell size.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid", meta = (ClampMin = "0.05", ClampMax = "0.9"))
    float SpikeDiameterFraction = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap|Spike Grid", meta = (ClampMin = "0.0", ClampMax = "20.0", Units = "cm"))
    float SpikeSurfaceOffset = 0.5f;

    // Generated fields receive this automatically. Also used for manually authored
    // spikes if Generate Spikes From Grid is disabled.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap")
    FName SpikeTag = TEXT("PipeTrapSpike");

    // Duration of the eased 180-degree panel turn.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap", meta = (ClampMin = "0.1", ClampMax = "10.0", Units = "s"))
    float TurnSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap", meta = (ClampMin = "0.1", ClampMax = "120.0", Units = "s"))
    float CloseSeconds = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe Trap", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "cm"))
    float FinalTipGap = 2.0f;
};

class AM_2610_API FMazePipeTrap
{
public:
    // One cone per fitted grid cell. Each outside panel receives its own instanced
    // field so the cones follow that panel's independent 180 degree flip.
    bool BuildSpikes(AActor* Room, const FMazePipeTrapSettings& Settings,
        const FMazePipeGridFrame& LeftFrame, const FMazePipeGridFrame& RightFrame,
        USceneComponent* LeftPipeParent, USceneComponent* RightPipeParent,
        float PipeWallOffset, bool bPreview, FString& Error);
    void ClearSpikes();

    // Pipes follow their individual inside panel, rather than a rotating wall root.
    USceneComponent* FindPipePanelParent(AActor* Room, const FMazePipeTrapSettings& Settings,
        bool bRightWall, const FMazePipeGridFrame& Frame, float PipeWallOffset,
        const FVector& Cell, FString& Error) const;

    bool Start(AActor* Room, const FMazePipeTrapSettings& Settings,
        const FMazePipeGridFrame& LeftFrame, const FMazePipeGridFrame& RightFrame,
        USceneComponent* LeftPipeParent, USceneComponent* RightPipeParent, float PipeWallOffset, FString& Error,
        const UBoxComponent* RoomArea = nullptr);

    // While Sequencer owns the player, only the turn can advance.
    // Returns true once on player-body contact. The puzzle component owns TriggerLose.
    bool Advance(float DeltaTime, bool bAllowContact = true);
    // Sequencer calls this under blackout. No closing or contact is advanced here.
    bool SwitchToSpikes();
    bool AreWallsSwitched() const { return bWallsSwitched; }
    bool IsActive() const { return bStarted && !bHitReported; }
    EMazePipeTrapPhase GetPhase() const { return Phase; }
    void Clear();

    // Rotate a panel about its pair's mesh center even if its component pivot is offset.
    static FTransform MakePose(const FTransform& Original, const FVector& Pivot,
        const FVector& Up, const FVector& Inward, float Radians, float Travel);

private:
    struct FPanel
    {
        TWeakObjectPtr<USceneComponent> Inside;
        TWeakObjectPtr<USceneComponent> Outside;
        FTransform InsideWorld;
        FTransform OutsideWorld;
        FTransform InsideRelative;
        FTransform OutsideRelative;
        FBox InsideBounds = FBox(ForceInit);
        FBox OutsideBounds = FBox(ForceInit);
        FVector Pivot = FVector::ZeroVector;
        FVector Axis = FVector::UpVector;
    };
    struct FWall
    {
        TWeakObjectPtr<USceneComponent> Root;
        FTransform OriginalWorld;
        FTransform OriginalRelative;
        FVector Pivot = FVector::ZeroVector;
        FVector Up = FVector::UpVector;
        FVector Inward = FVector::ForwardVector;
        FTransform ContactSpace;
        FBox ContactBounds = FBox(ForceInit);
        float Travel = 0.0f;
        TArray<FPanel> Panels;
    };
    struct FSceneState
    {
        TWeakObjectPtr<USceneComponent> Component;
        EComponentMobility::Type Mobility = EComponentMobility::Static;
    };
    struct FSpikeCollisionState
    {
        TWeakObjectPtr<UPrimitiveComponent> Component;
        ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
    };

    TWeakObjectPtr<AActor> Owner;
    FMazePipeTrapSettings ActiveSettings;
    FWall Walls[2];
    TArray<FSceneState> Scenes;
    TArray<FSpikeCollisionState> SpikeCollisions;
    float Elapsed = 0.0f;
    bool bStarted = false;
    bool bWallsSwitched = false;
    bool bHitReported = false;
    EMazePipeTrapPhase Phase = EMazePipeTrapPhase::Idle;
    TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> GeneratedSpikes;

    static bool IsSpike(const USceneComponent* Component, const USceneComponent* Root, FName Tag);
    static USceneComponent* FindRoot(AActor* Room, FName Tag, FString& Error);
    static FTransform GetGridSpace(const FMazePipeGridFrame& Frame, float PipeWallOffset);
    static bool ReadPanelBounds(AActor* Room, USceneComponent* Panel, const FTransform& Space,
        FName SpikeTag, FBox& Bounds, FString& Error);
    static bool ReadPanels(AActor* Room, USceneComponent* Root, const FMazePipeGridFrame& Frame,
        float PipeWallOffset, const FMazePipeTrapSettings& Settings, TArray<FPanel>& Panels, FString& Error);
    static int32 FindPanelIndex(const TArray<FPanel>& Panels, const FVector& Point, bool bOutside);
    static bool ReadFlippedSpikeBounds(const FWall& Wall, int32 Side, FName SpikeTag, FBox& Bounds, FString& Error);
    void ClearMotion();
    bool ReadWall(AActor* Room, FName Tag, FName SpikeTag, FWall& Wall, FString& Error);
    void ApplyPose(float TurnAlpha, float CloseAlpha);
    bool HasPlayerContact(float PreviousCloseAlpha, float CloseAlpha) const;
};

// Synchronous console input lock. Sequencer owns all cinematic timing, lights,
// sound and look keys. No fade, release timer or camera cut is performed here.
class AM_2610_API FMazePipeTrapReveal
{
public:
    bool LockPlayer(APawn* Player, FString& Error);
    void ApplyLook(float YawOffset, float Pitch);
    void UnlockPlayer();
    void Clear();
    bool IsPlayerLocked() const { return bLocked; }
    bool HasValidPlayer() const;
    APawn* GetPlayer() const { return Pawn.Get(); }
    APlayerController* GetController() const { return Controller.Get(); }

private:
    void ReleasePlayer(bool bRestoreLook);
    TWeakObjectPtr<APawn> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UPawnMovementComponent> Movement;
    FRotator OriginalLook = FRotator::ZeroRotator;
    uint8 OriginalMovementMode = 0;
    uint8 OriginalCustomMovementMode = 0;
    bool bMovementWasActive = false;
    bool bPawnInputWasEnabled = false;
    bool bLocked = false;
};