// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePuzzleComponent.h"
#include "Actors/BaseGameMode.h"
#include "Enviornment/MazePipePiece.h"
#include "Enviornment/MazePipeFitting.h"
#include "Math/Box.h"
#include "Math/RotationMatrix.h"
#include "Enviornment/MazeActors.h"
#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeInterface.h"
#include "Enviornment/MazePuzzleButton.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Components/LineBatchComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/LightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
    bool IsRoomAreaEnabled(const UBoxComponent& Area)
    {
        return Area.K2_IsQueryCollisionEnabled() && Area.GetGenerateOverlapEvents()
            && Area.GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap;
    }

    bool IsPlayerFullyInsideRoomArea(const UBoxComponent& Area, const APawn& Player)
    {
        const FTransform& Transform = Area.GetComponentTransform();
        FVector Center;
        FVector Extent;
        if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Player.GetRootComponent()))
        {
            Center = Capsule->GetComponentLocation();
            float Radius;
            float HalfHeight;
            Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
            const FVector Axis = Transform.InverseTransformVectorNoScale(Capsule->GetUpVector()).GetAbs();
            Extent = FVector(Radius) + Axis * FMath::Max(0.0f, HalfHeight - Radius);
        }
        else
        {
            FVector WorldExtent;
            Player.GetActorBounds(true, Center, WorldExtent);
            Extent = FVector(
                FVector::DotProduct(WorldExtent, Area.GetForwardVector().GetAbs()),
                FVector::DotProduct(WorldExtent, Area.GetRightVector().GetAbs()),
                FVector::DotProduct(WorldExtent, Area.GetUpVector().GetAbs()));
        }

        const FVector Farthest = Transform.InverseTransformPositionNoScale(Center).GetAbs() + Extent;
        const FVector BoxExtent = Area.GetScaledBoxExtent();
        return Farthest.X <= BoxExtent.X && Farthest.Y <= BoxExtent.Y && Farthest.Z <= BoxExtent.Z;
    }
}

UMazePuzzleComponent::UMazePuzzleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 0.05f;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    DefaultPipeSpikeMesh = Cone.Object;
#if WITH_EDITOR
    bTickInEditor = true;
#endif
}

bool UMazePuzzleComponent::PreparePuzzle(AMazeGenerator* Generator, FName SelectedId, TSubclassOf<APawn> AngelClass, int32 Seed, bool bSpawnAllButtons, bool bSpawnAllEnemies)
{
    AMazeActors* Room = Cast<AMazeActors>(GetOwner());

    if (bPrepared || !IsValid(Generator) || !IsValid(Room) || SelectedId.IsNone() || !Room->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()) || EnemySpawnTag.IsNone() || PuzzleDoorTag.IsNone() || ButtonSpawnTag.IsNone() || ButtonSpawnTag == EnemySpawnTag)
    {
        return false;
    }

    if (bUsePipePuzzle && bUsePressureButtons)
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Disable Use Pressure Buttons on the pipe room component."));
        return false;
    }

    bSpawnAllEnemies = bSpawnAllEnemies && !bUsePipePuzzle;
    RequestedEnemyCount = bUsePipePuzzle ? 0 : FMath::Clamp(WeepingAngelCount, 0, 16);

    if ((bUsePressureButtons && (!ButtonClass || !AngelClass)) || ((RequestedEnemyCount > 0 || bSpawnAllEnemies) && !AngelClass))
    {
        UE_LOG(LogTemp, Error, TEXT("Puzzle: Assign ButtonClass and the generator's Weeping Angel enemy tag."));
        return false;
    }

    TArray<UArrowComponent*> Arrows;
    Room->GetComponents<UArrowComponent>(Arrows);
    Arrows.Sort([](const UArrowComponent& A, const UArrowComponent& B)
        {
            return A.GetName() < B.GetName();
        });

    for (UArrowComponent* Arrow : Arrows)
    {
        const bool bButton = Arrow->ComponentHasTag(ButtonSpawnTag);
        const bool bEnemy = Arrow->ComponentHasTag(EnemySpawnTag);

        // Spawn markers are separate from hallway connections and from each other.
        if ((bButton || bEnemy) && (Arrow->ComponentHasTag(TEXT("MazeConnection")) || (bButton && bEnemy)))
        {
            UE_LOG(LogTemp, Error, TEXT("Puzzle: Spawn arrows must use only one puzzle spawn tag and no MazeConnection tag."));
            return false;
        }

        if (bButton)
        {
            ButtonSpawns.Add(Arrow);
        }

        if (bEnemy)
        {
            EnemySpawns.Add(Arrow);
        }
    }

    if ((bUsePressureButtons && ButtonSpawns.Num() < 2) || (!bSpawnAllEnemies && EnemySpawns.Num() < RequestedEnemyCount) || (bSpawnAllEnemies && EnemySpawns.IsEmpty()))
    {
        UE_LOG(LogTemp, Error, TEXT("Puzzle: Provide two button arrows and enough enemy arrows. Enemy debug mode needs at least one enemy arrow."));
        return false;
    }

    // Validate the entry trigger now, while keeping all Angels unspawned.
    if ((RequestedEnemyCount > 0 || bSpawnAllEnemies) && !PrepareEnemyEntryTrigger())
    {
        return false;
    }

    if (bUsePipePuzzle && !GeneratePipeLayouts(Seed))
    {
        return false;
    }

    FRandomStream Random(Seed);

    for (int32 Index = ButtonSpawns.Num() - 1; Index > 0; --Index)
    {
        ButtonSpawns.Swap(Index, Random.RandRange(0, Index));
    }

    for (int32 Index = EnemySpawns.Num() - 1; Index > 0; --Index)
    {
        EnemySpawns.Swap(Index, Random.RandRange(0, Index));
    }

    // Debug mode shows every marker, but keeps the normal two-button puzzle.
    bDebugSpawnAllButtons = bUsePressureButtons && bSpawnAllButtons;
    bDebugSpawnAllEnemies = bSpawnAllEnemies;
    RequestedButtonCount = bUsePressureButtons ? (bDebugSpawnAllButtons ? ButtonSpawns.Num() : 2) : 0;
    MazeGenerator = Generator;
    PuzzleId = SelectedId;
    WeepingAngelClass = AngelClass;
    bPuzzleStarted = false;
    bButtonsUnlocked = !bUsePressureButtons;
    bPuzzleDoorUnlocked = true;
    bConsoleRed = false;
    bPrepared = true;
    bPuzzleDoorSealed = false;
    PrepareCompletedExitArea();
    InitializeStatusLights();
    IMazeInterface::Execute_SetMazeDoorLocked(Room, PuzzleDoorTag, false);
    UpdateStatusLights();
    if (!InitializePipeFailureTallies())
    {
        bPrepared = false;
        return false;
    }
    OnConsoleStateChanged.Broadcast(false);
    return true;
}

bool UMazePuzzleComponent::StepButtonSpawning(bool& bFinished)
{
    bFinished = !bUsePressureButtons || SpawnedButtons.Num() + DebugButtons.Num() == RequestedButtonCount;

    if (!bPrepared || !GetWorld())
    {
        return false;
    }

    if (bFinished)
    {
        return true;
    }

    if (ButtonSpawnIndex >= ButtonSpawns.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Puzzle: Buttons placed %d/%d in %s. Check the failed button arrows above."), SpawnedButtons.Num() + DebugButtons.Num(), RequestedButtonCount, *GetNameSafe(GetOwner()));
        return false;
    }

    UArrowComponent* Marker = ButtonSpawns[ButtonSpawnIndex++].Get();

    if (IsValid(Marker))
    {
        FActorSpawnParameters Parameters;
        Parameters.Owner = GetOwner();
        // Plates sit against the room geometry. Keep them exactly at their arrows.
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AMazePuzzleButton* Button = GetWorld()->SpawnActor<AMazePuzzleButton>(ButtonClass, Marker->GetComponentLocation(), Marker->GetComponentRotation(), Parameters);

        if (IsValid(Button))
        {
            USceneComponent* StandingArea = Button->GetRootComponent();

            if (!IsValid(StandingArea))
            {
                UE_LOG(LogTemp, Warning, TEXT("Puzzle: Button spawn failed at %s (%s) in %s. The button has no root standing area."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
                Button->Destroy();
                return true;
            }

            // Compare only the root ButtonArea boxes, not the decorative mesh bounds.
            const FBox StandingBounds = StandingArea->Bounds.GetBox();
            const bool bOverlapsButton = !bDebugSpawnAllButtons && SpawnedButtons.ContainsByPredicate([StandingBounds](const AMazePuzzleButton* Other)
                {
                    return IsValid(Other) && IsValid(Other->GetRootComponent()) && StandingBounds.Intersect(Other->GetRootComponent()->Bounds.GetBox());
                });

            if (bOverlapsButton)
            {
                UE_LOG(LogTemp, Warning, TEXT("Puzzle: Button spawn rejected at %s (%s) in %s. Its standing area overlaps another selected button."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
                Button->Destroy();
            }
            else
            {
                if (SpawnedButtons.Num() < 2)
                {
                    SpawnedButtons.Add(Button);
                }
                else
                {
                    // Preview buttons must not block the player or affect the puzzle.
                    Button->SetActorEnableCollision(false);
                    DebugButtons.Add(Button);
                }

#if WITH_EDITOR
                if (bDebugSpawnAllButtons)
                {
                    Button->SetActorLabel(FString::Printf(TEXT("PuzzleButton_%s"), *Marker->GetName()), false);
                }
#endif
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Puzzle: Button spawn failed at %s (%s) in %s. The button class could not be spawned."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Button spawn failed in %s. Button arrow %d is invalid."), *GetNameSafe(GetOwner()), ButtonSpawnIndex);
    }

    bFinished = SpawnedButtons.Num() + DebugButtons.Num() == RequestedButtonCount;
    return true;
}

bool UMazePuzzleComponent::StepEnemySpawning(bool& bFinished)
{
    bFinished = bDebugSpawnAllEnemies ? EnemySpawnIndex >= EnemySpawns.Num() : SpawnedWeepingAngels.Num() == RequestedEnemyCount;

    if (!bPrepared || !GetWorld())
    {
        return false;
    }

    // The generator can finish while this prepared room waits for player entry.
    if (bWaitingForEnemyEntry)
    {
        bFinished = true;
        return true;
    }

    if (bFinished)
    {
        return true;
    }

    if (EnemySpawnIndex >= EnemySpawns.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Puzzle: Weeping Angels placed %d/%d in %s. Check the failed spawn arrows above."), SpawnedWeepingAngels.Num(), RequestedEnemyCount, *GetNameSafe(GetOwner()));
        return false;
    }

    UArrowComponent* Marker = EnemySpawns[EnemySpawnIndex++].Get();

    // Debug mode checks every marker once, keeping successful spawns for inspection.
    const auto FinishAttempt = [this, &bFinished]()
        {
            bFinished = bDebugSpawnAllEnemies ? EnemySpawnIndex >= EnemySpawns.Num() : SpawnedWeepingAngels.Num() == RequestedEnemyCount;

            if (bDebugSpawnAllEnemies && bFinished)
            {
                UE_LOG(LogTemp, Display, TEXT("Puzzle: Weeping Angel spawn debug finished in %s. Succeeded: %d/%d. Failed: %d."), *GetNameSafe(GetOwner()), SpawnedWeepingAngels.Num(), EnemySpawns.Num(), EnemySpawns.Num() - SpawnedWeepingAngels.Num());
            }

            return true;
        };

    if (!IsValid(Marker))
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Weeping Angel spawn failed in %s. Enemy arrow %d is invalid."), *GetNameSafe(GetOwner()), EnemySpawnIndex);
        return FinishAttempt();
    }

    UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
    const APawn* Defaults = WeepingAngelClass.GetDefaultObject();
    ANavigationData* NavData = Navigation && Defaults ? Navigation->GetNavDataForProps(Defaults->GetNavAgentPropertiesRef()) : nullptr;
    FNavLocation NavigableSpawn;

    if (!NavData || NavData->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic || !Navigation->ProjectPointToNavigation(Marker->GetComponentLocation(), NavigableSpawn, FVector(50.0, 50.0, 500.0), NavData))
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Weeping Angel spawn failed at %s (%s) in %s. No dynamic NavMesh was found below this marker."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
        return FinishAttempt();
    }

    FActorSpawnParameters Parameters;
    Parameters.Owner = GetOwner();
    // Spawn only at the arrow. A blocked location must fail instead of moving the enemy.
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    APawn* Enemy = GetWorld()->SpawnActor<APawn>(WeepingAngelClass, Marker->GetComponentLocation(), Marker->GetComponentRotation(), Parameters);

    if (!IsValid(Enemy))
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Weeping Angel spawn failed at %s (%s) in %s. Spawn was blocked or rejected at this exact location."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
        return FinishAttempt();
    }

    if (!Enemy->GetController())
    {
        Enemy->SpawnDefaultController();
    }

    if (!Enemy->GetController())
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Weeping Angel spawn failed at %s (%s) in %s. No AI Controller was created; the enemy was removed."), *Marker->GetName(), *Marker->GetComponentLocation().ToString(), *GetNameSafe(GetOwner()));
        Enemy->Destroy();
        return FinishAttempt();
    }

    SpawnedWeepingAngels.Add(Enemy);

#if WITH_EDITOR
    if (bDebugSpawnAllEnemies)
    {
        Enemy->SetActorLabel(FString::Printf(TEXT("PuzzleAngel_%s"), *Marker->GetName()), false);
    }
#endif

    return FinishAttempt();
}

bool UMazePuzzleComponent::PrepareEnemyEntryTrigger()
{
    AMazeActors* Room = Cast<AMazeActors>(GetOwner());

    if (!IsValid(Room) || EnemySpawnTriggerTag.IsNone())
    {
        return false;
    }

    TArray<UBoxComponent*> Boxes;
    Room->GetComponents<UBoxComponent>(Boxes);
    UBoxComponent* Trigger = nullptr;

    for (UBoxComponent* Box : Boxes)
    {
        if (!IsValid(Box) || !Box->ComponentHasTag(EnemySpawnTriggerTag))
        {
            continue;
        }

        if (Trigger || Box->ComponentHasTag(TEXT("MazeBounds")) || Box->ComponentHasTag(TEXT("RoomArrival")) || Box->ComponentHasTag(TEXT("RoomArea")) || Box->GetFName() == FName(TEXT("RoomArea")))
        {
            UE_LOG(LogTemp, Error, TEXT("Puzzle: %s needs exactly one separate box tagged %s. Do not reuse MazeBounds, RoomArrival, or RoomArea."), *GetNameSafe(Room), *EnemySpawnTriggerTag.ToString());
            return false;
        }

        Trigger = Box;
    }

    if (!IsValid(Trigger))
    {
        UE_LOG(LogTemp, Error, TEXT("Puzzle: %s needs a Box Collision component tagged %s for Angel spawning on player entry."), *GetNameSafe(Room), *EnemySpawnTriggerTag.ToString());
        return false;
    }

    ClearEnemyEntryTrigger();
    EnemyEntryTrigger = Trigger;
    bWaitingForEnemyEntry = true;

    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->SetCanEverAffectNavigation(false);
    Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &UMazePuzzleComponent::OnEnemyEntryTriggerBeginOverlap);
    return true;
}

void UMazePuzzleComponent::EnableEnemyEntryTrigger()
{
    UBoxComponent* Trigger = EnemyEntryTrigger.Get();

    if (!bArmed || !bWaitingForEnemyEntry || !IsValid(Trigger) || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return;
    }

    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    TArray<AActor*> OverlappingActors;
    Trigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

    for (AActor* Actor : OverlappingActors)
    {
        OnEnemyEntryTriggerBeginOverlap(Trigger, Actor, nullptr, INDEX_NONE, false, FHitResult());
    }
}

void UMazePuzzleComponent::OnEnemyEntryTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* Player = Cast<APawn>(OtherActor);

    if (!bArmed || bPuzzleCompleted || ShouldWatchRoomVisit() || !bWaitingForEnemyEntry
        || !IsValid(OverlappedComponent) || OverlappedComponent != EnemyEntryTrigger.Get()
        || OverlappedComponent->GetOwner() != GetOwner() || !IsValid(Player) || !Player->IsPlayerControlled()
        || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return;
    }

    // Latch before spawning; additional player components and re-entry cannot duplicate it.
    bWaitingForEnemyEntry = false;
    bEnemySpawningPending = true;
    EnemySpawnRetryRemaining = 0.0f;

    UBoxComponent* Trigger = EnemyEntryTrigger.Get();
    Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &UMazePuzzleComponent::OnEnemyEntryTriggerBeginOverlap);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Keep the pressure-room door and console behavior when entry starts the Angels.
    if (bUsePressureButtons && !bPuzzleStarted)
    {
        BeginPuzzle(Player);
    }

    UE_LOG(LogTemp, Display, TEXT("Puzzle: Player entered %s. Starting the room's Weeping Angel spawns."), *GetNameSafe(GetOwner()));

    AdvanceEntryEnemySpawning(0.0f);
    SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive() || bPipeSteamChecking || bEnemySpawningPending || (bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked));
}

void UMazePuzzleComponent::AdvanceEntryEnemySpawning(float DeltaTime)
{
    if (!bArmed || !bEnemySpawningPending || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return;
    }

    EnemySpawnRetryRemaining = FMath::Max(0.0f, EnemySpawnRetryRemaining - FMath::Max(0.0f, DeltaTime));

    if (EnemySpawnRetryRemaining > 0.0f)
    {
        return;
    }

    bool bFinished = false;

    if (!StepEnemySpawning(bFinished))
    {
        // A player may temporarily block a spawn after entry. Retry without rebuilding
        // the occupied maze or discarding the Angels already spawned successfully.
        EnemySpawnIndex = 0;
        EnemySpawnRetryRemaining = 1.0f;
    }
    else if (bFinished)
    {
        bEnemySpawningPending = false;
    }
}

void UMazePuzzleComponent::ClearEnemyEntryTrigger()
{
    if (UBoxComponent* Trigger = EnemyEntryTrigger.Get())
    {
        Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &UMazePuzzleComponent::OnEnemyEntryTriggerBeginOverlap);
        Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    EnemyEntryTrigger.Reset();
    bWaitingForEnemyEntry = false;
    bEnemySpawningPending = false;
    EnemySpawnRetryRemaining = 0.0f;
}

void UMazePuzzleComponent::ArmPuzzle()
{
    // A validated entry trigger counts as ready while its Angels wait for the player.
    const bool bEnemiesReady = bWaitingForEnemyEntry || bEnemySpawningPending || (bDebugSpawnAllEnemies ? EnemySpawnIndex >= EnemySpawns.Num() : SpawnedWeepingAngels.Num() == RequestedEnemyCount);
    bArmed = bPrepared && (!bUsePressureButtons || SpawnedButtons.Num() == 2) && bEnemiesReady;
    const bool bWatchRoom = ShouldWatchRoomVisit();
    SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive() || bPipeSteamChecking
        || bWatchRoom || (bArmed && (bEnemySpawningPending || (bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked))));
    if (!bWatchRoom)
    {
        EnableEnemyEntryTrigger();
    }
}

bool UMazePuzzleComponent::BeginPuzzle(APawn* Player)
{
    const bool bReturningToPipePuzzle = bUsePipePuzzle && bPuzzleDoorUnlocked && !ArePipeWallsCompleted();
    if (!bArmed || (!bUsePressureButtons && !bUsePipePuzzle) || bPipeTrapTriggered
        || (bPuzzleStarted && !bReturningToPipePuzzle) || bPuzzleCompleted
        || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid()
        || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
    }

    const UBoxComponent* Area = CompletedExitArea.Get();
    if (Area && (!IsRoomAreaEnabled(*Area) || !IsPlayerFullyInsideRoomArea(*Area, *Player)))
    {
        return false;
    }

    bPuzzleStarted = true;
    bPuzzleDoorUnlocked = false;
    bConsoleRed = bUsePressureButtons && !bButtonsUnlocked;
    IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, true);
    UpdateStatusLights();
    OnConsoleStateChanged.Broadcast(bConsoleRed);
    SetComponentTickEnabled(true);
    return true;
}

bool UMazePuzzleComponent::IsStandalonePipeTest() const
{
    return bAllowStandalonePipeTesting && bUsePipePuzzle && !bUsePressureButtons
        && !bPrepared && !bArmed && !MazeGenerator.IsValid()
        && GetWorld() && GetWorld()->IsGameWorld();
}

bool UMazePuzzleComponent::CanActivateConsole(APawn* Player) const
{
    if (bPuzzleCompleted || (bUsePipePuzzle && bPipeTrapTriggered) || !IsValid(Player) || !Player->IsPlayerControlled()
        || !GetWorld() || !GetWorld()->IsGameWorld() || Player->GetWorld() != GetWorld())
    {
        return false;
    }
    if (IsStandalonePipeTest())
    {
        return !bPipeSteamChecking && (bLeftPipeActive || bRightPipeActive) && !ArePipeWallsCompleted();
    }
    if (!bArmed || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
    }
    if (bUsePipePuzzle)
    {
        const UBoxComponent* Area = CompletedExitArea.Get();
        return !bUsePressureButtons && PipeFailureTallyCount > 0 && !bPipeSteamChecking && (bLeftPipeActive || bRightPipeActive)
            && (!Area || (IsRoomAreaEnabled(*Area) && IsPlayerFullyInsideRoomArea(*Area, *Player)));
    }
    return bPuzzleStarted && bButtonsUnlocked && !bPuzzleDoorUnlocked;
}

bool UMazePuzzleComponent::ActivateConsole(APawn* Player)
{
    if (!CanActivateConsole(Player))
    {
        return false;
    }
    if (bUsePipePuzzle)
    {
        // A placed test board runs the same wave without locking or completing a maze room.
        if (IsStandalonePipeTest())
        {
            return CheckPipePuzzle();
        }
        // Allow a verified result to retry registration if the generator previously rejected it.
        if (ArePipeWallsCompleted() && bPipeSteamResultShown && !bPipeCheckUsesPreview)
        {
            return CompletePuzzle();
        }
        if (!CheckPipePuzzle())
        {
            return false;
        }
        // The first valid check can also lock the door after the player enters.
        if ((!bPuzzleStarted || bPuzzleDoorUnlocked) && !BeginPuzzle(Player))
        {
            ResetPipeLights();
            return false;
        }
        const bool bTerminalFailure = PipeFailureTallyCount > 0
            && PipeFailedSteamChecks == PipeFailureTallyCount - 1
            && ((bLeftPipeActive && !bLeftPipeCompleted && !LeftPipeSteamResult.bPassed)
                || (bRightPipeActive && !bRightPipeCompleted && !RightPipeSteamResult.bPassed));
        if (bTerminalFailure)
        {
            if (!LockPipeTrapPlayer(Player))
            {
                ResetPipeLights();
                return false;
            }
            // This one failed check resolves in the console call. The player is
            // already locked when the final tally requests Sequencer playback.
            PipeSteamStep = LastPipeSteamStep;
            ApplyPipeSteamStep();
            AdvancePipeSteamCheck(0.0f);
        }
        return true;
    }

    // Readiness stays latched while the player walks from the button to the console.
    bPuzzleDoorUnlocked = true;
    IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, false);
    UpdateStatusLights();
    OnConsoleActivated.Broadcast();
    return true;
}

bool UMazePuzzleComponent::IsEligibleOccupant(APawn* Pawn) const
{
    return IsValid(Pawn) && (Pawn->IsPlayerControlled() || (WeepingAngelClass && Pawn->IsA(WeepingAngelClass.Get())));
}

bool UMazePuzzleComponent::CanUnlockButtons(const TArray<APawn*>& First, const TArray<APawn*>& Second) const
{
    for (APawn* A : First)
    {
        for (APawn* B : Second)
        {
            if (!IsValid(A) || !IsValid(B) || A == B || !WeepingAngelClass)
            {
                continue;
            }

            // One real player and one Angel must occupy different buttons together.
            if ((A->IsPlayerControlled() && B->IsA(WeepingAngelClass.Get())) || (B->IsPlayerControlled() && A->IsA(WeepingAngelClass.Get())))
            {
                return true;
            }
        }
    }

    return false;
}

void UMazePuzzleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (PipeTrapReveal.IsPlayerLocked() && (!PipeTrapReveal.HasValidPlayer()
        || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze))
    {
        ReleasePipeTrapPlayer();
        if (!bPipeTrapTriggered) { ResetPipeLights(); }
        SetComponentTickEnabled(false);
        return;
    }
    const bool bTrapWasTriggered = bPipeTrapTriggered;
    AdvancePipeSteamCheck(DeltaTime);
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        SetComponentTickEnabled(bPipeSteamChecking);
        return;
    }

    if (bPipeTrapTriggered)
    {
        AdvancePipeTrap(bTrapWasTriggered ? DeltaTime : 0.0f);
        return;
    }

    const bool bWatchRoom = ShouldWatchRoomVisit();
    if (bWatchRoom && !UpdateRoomVisit(*CompletedExitArea.Get()))
    {
        return;
    }

    AdvanceEntryEnemySpawning(DeltaTime);

    if (!bArmed || !bPuzzleStarted || bButtonsUnlocked || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze || SpawnedButtons.Num() != 2 || !IsValid(SpawnedButtons[0]) || !IsValid(SpawnedButtons[1]))
    {
        if (!bWatchRoom && !bEnemySpawningPending && !bPipeSteamChecking)
        {
            SetComponentTickEnabled(false);
        }

        return;
    }

    TArray<APawn*> First;
    TArray<APawn*> Second;
    SpawnedButtons[0]->GetOverlappingPawns(First);
    SpawnedButtons[1]->GetOverlappingPawns(Second);

    SpawnedButtons[0]->SetPressed(First.ContainsByPredicate([this](APawn* Pawn) { return IsEligibleOccupant(Pawn); }));
    SpawnedButtons[1]->SetPressed(Second.ContainsByPredicate([this](APawn* Pawn) { return IsEligibleOccupant(Pawn); }));

    if (CanUnlockButtons(First, Second))
    {
        // Turn the console blue, but keep the door locked until it is used.
        bButtonsUnlocked = true;
        bConsoleRed = false;
        SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive()
            || bPipeSteamChecking || bEnemySpawningPending || bWatchRoom);
        OnConsoleStateChanged.Broadcast(false);
        OnButtonsUnlocked.Broadcast();
    }
}

bool UMazePuzzleComponent::CompletePuzzle()
{
    if (!bArmed || bPuzzleCompleted || !MazeGenerator.IsValid())
    {
        return false;
    }
    if (bUsePipePuzzle)
    {
        if (bUsePressureButtons || !bPuzzleStarted || !GetWorld() || !GetWorld()->IsGameWorld()
            || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze || bPipeCheckUsesPreview || bPipeCheckIsStandalone
            || bPipeSteamChecking || bPipeTrapTriggered || !bPipeSteamResultShown || !ArePipeWallsCompleted())
        {
            return false;
        }
    }
    else if (!bButtonsUnlocked || !bPuzzleDoorUnlocked || (bUsePressureButtons && !bPuzzleStarted))
    {
        return false;
    }
    // The shared generator requires the logical door flag before accepting completion.
    // Physical unlocking and success events stay after registration succeeds.
    const bool bPreviouslyDoorUnlocked = bPuzzleDoorUnlocked;
    if (bUsePipePuzzle)
    {
        bPuzzleDoorUnlocked = true;
    }
    if (!MazeGenerator->RegisterPuzzleCompleted(Cast<AMazeActors>(GetOwner())))
    {
        bPuzzleDoorUnlocked = bPreviouslyDoorUnlocked;
        return false;
    }

    bPuzzleCompleted = true;
    if (bUsePipePuzzle)
    {
        bPuzzleDoorUnlocked = true;
        bConsoleRed = false;
        IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, false);
    }
    UpdateStatusLights();
    SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive() || bPipeSteamChecking || bEnemySpawningPending);
    QueueCompletedExitCheck();
    if (bUsePipePuzzle)
    {
        OnConsoleStateChanged.Broadcast(false);
        // Reuse the pressure console's successful unlock and door opening event.
        OnConsoleActivated.Broadcast();
    }
    OnPuzzleCompleted.Broadcast();
    return true;
}

void UMazePuzzleComponent::InitializeStatusLights()
{
    StatusLightMaterials.Reset();
    StatusLightMeshes.Reset();
    CachedStatusMaterialIndex = StatusLightMaterialIndex;
    AActor* Room = GetOwner();
    if (!IsValid(Room) || StatusLightComponentTag.IsNone() || StatusLightColorParameter.IsNone() || CachedStatusMaterialIndex < 0)
    {
        return;
    }

    TArray<UMeshComponent*> Meshes;
    Room->GetComponents<UMeshComponent>(Meshes, true);
    for (UMeshComponent* Mesh : Meshes)
    {
        if (!IsValid(Mesh) || !Mesh->ComponentHasTag(StatusLightComponentTag))
        {
            continue;
        }
        if (CachedStatusMaterialIndex >= Mesh->GetNumMaterials())
        {
            UE_LOG(LogTemp, Warning, TEXT("Puzzle: Tagged status mesh %s has no material slot %d."), *Mesh->GetName(), CachedStatusMaterialIndex);
            continue;
        }
        UMaterialInterface* Source = Mesh->GetMaterial(CachedStatusMaterialIndex);
        if (!IsValid(Source))
        {
            continue;
        }
        // Keep any constant-instance permutation, but never parent a new MID to
        // an existing MID. Copy its uniform overrides into a private instance.
        UMaterialInterface* Parent = Source;
        while (UMaterialInstanceDynamic* DynamicParent = Cast<UMaterialInstanceDynamic>(Parent))
        {
            Parent = DynamicParent->Parent;
        }
        if (!IsValid(Parent))
        {
            continue;
        }
        if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this))
        {
            Material->CopyMaterialUniformParameters(Source);
            Mesh->SetMaterial(CachedStatusMaterialIndex, Material);
            StatusLightMeshes.Add(Mesh);
            StatusLightMaterials.Add(Material);
        }
    }
    UpdateStatusLights();
}

void UMazePuzzleComponent::UpdateStatusLights()
{
    const FLinearColor Color = bPuzzleCompleted ? CompletedLightColor : IncompleteLightColor;
    for (int32 Index = 0; Index < StatusLightMaterials.Num(); ++Index)
    {
        UMaterialInstanceDynamic* Material = StatusLightMaterials[Index];
        UMeshComponent* Mesh = StatusLightMeshes.IsValidIndex(Index) ? StatusLightMeshes[Index].Get() : nullptr;
        if (IsValid(Material) && IsValid(Mesh) && CachedStatusMaterialIndex >= 0 && CachedStatusMaterialIndex < Mesh->GetNumMaterials())
        {
            // Completion owns these tagged indicators, including after a lock update.
            if (Mesh->GetMaterial(CachedStatusMaterialIndex) != Material)
            {
                Mesh->SetMaterial(CachedStatusMaterialIndex, Material);
            }
            Material->SetVectorParameterValue(StatusLightColorParameter, Color);
        }
    }
}

void UMazePuzzleComponent::PrepareCompletedExitArea()
{
    ClearCompletedExitArea();
    AActor* Room = GetOwner();
    if (!IsValid(Room) || CompletedExitAreaTag.IsNone())
    {
        return;
    }

    TArray<UBoxComponent*> Boxes;
    Room->GetComponents<UBoxComponent>(Boxes);
    UBoxComponent* Area = nullptr;
    for (UBoxComponent* Box : Boxes)
    {
        if (!IsValid(Box) || (!Box->ComponentHasTag(CompletedExitAreaTag) && Box->GetFName() != CompletedExitAreaTag))
        {
            continue;
        }
        if (Area || Box->ComponentHasTag(TEXT("MazeBounds")) || Box->ComponentHasTag(TEXT("RoomArrival")) || Box == EnemyEntryTrigger.Get())
        {
            UE_LOG(LogTemp, Warning, TEXT("Puzzle: Use one whole-room box tagged %s for completion departure; keep it separate from placement, arrival, and enemy-entry boxes."), *CompletedExitAreaTag.ToString());
            return;
        }
        Area = Box;
    }
    if (!IsValid(Area))
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: No whole-room box tagged/named %s; the completed door cannot seal on departure."), *CompletedExitAreaTag.ToString());
        return;
    }
    const FVector Extent = Area->GetUnscaledBoxExtent();
    if (Extent.ContainsNaN() || Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0
        || !IsRoomAreaEnabled(*Area))
    {
        UE_LOG(LogTemp, Warning, TEXT("Puzzle: Completion room area needs nonzero dimensions, query collision, Pawn overlap, and overlap events."));
        return;
    }

    // Observe the room's existing area without changing its AI/room-state collision.
    CompletedExitArea = Area;
    Area->OnComponentBeginOverlap.AddUniqueDynamic(this, &UMazePuzzleComponent::OnCompletionAreaBeginOverlap);
    Area->OnComponentEndOverlap.AddUniqueDynamic(this, &UMazePuzzleComponent::OnCompletionAreaEndOverlap);
    TArray<AActor*> Occupants;
    Area->GetOverlappingActors(Occupants, APawn::StaticClass());
    for (AActor* Occupant : Occupants)
    {
        OnCompletionAreaBeginOverlap(Area, Occupant, nullptr, INDEX_NONE, false, FHitResult());
    }
}

void UMazePuzzleComponent::ClearCompletedExitArea()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CompletedExitTimer);
    }
    if (UBoxComponent* Area = CompletedExitArea.Get())
    {
        Area->OnComponentBeginOverlap.RemoveDynamic(this, &UMazePuzzleComponent::OnCompletionAreaBeginOverlap);
        Area->OnComponentEndOverlap.RemoveDynamic(this, &UMazePuzzleComponent::OnCompletionAreaEndOverlap);
    }
    CompletedExitArea.Reset();
    CompletedExitPlayer.Reset();
    bObservedPlayerInPuzzleArea = false;
    bRoomVisitActive = false;
}

bool UMazePuzzleComponent::ShouldWatchRoomVisit() const
{
    return bArmed && !bPuzzleCompleted && !bPipeTrapTriggered && CompletedExitArea.IsValid()
        && MazeGenerator.IsValid() && MazeGenerator->LoopState == EMazeLoopState::TraversingMaze;
}

bool UMazePuzzleComponent::UpdateRoomVisit(const UBoxComponent& Area)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsRoomAreaEnabled(Area) || !IsValid(Player) || !Player->IsPlayerControlled())
    {
        return false;
    }

    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Player->GetRootComponent());
    const bool bOverlapsRoom = Capsule ? Area.IsOverlappingComponent(Capsule) : Area.IsOverlappingActor(Player);
    if (bOverlapsRoom)
    {
        if (!IsPlayerFullyInsideRoomArea(Area, *Player))
        {
            return false;
        }

        if (!bRoomVisitActive)
        {
            bRoomVisitActive = true;
            if ((bUsePressureButtons && !bPuzzleStarted) || (bUsePipePuzzle && bPuzzleStarted && bPuzzleDoorUnlocked))
            {
                BeginPuzzle(Player);
            }
            if (bWaitingForEnemyEntry)
            {
                bWaitingForEnemyEntry = false;
                bEnemySpawningPending = true;
            }
        }
        return true;
    }

    if (!bRoomVisitActive && bPuzzleDoorUnlocked)
    {
        return false;
    }

    bRoomVisitActive = false;
    bEnemySpawningPending = false;
    bWaitingForEnemyEntry = RequestedEnemyCount > 0 || bDebugSpawnAllEnemies;
    EnemySpawnIndex = 0;
    EnemySpawnRetryRemaining = 0.0f;
    ClearSpawnedWeepingAngels();

    // An unused pressure console can lock the door again on the next visit.
    if (bUsePressureButtons && !bPuzzleDoorUnlocked)
    {
        bPuzzleStarted = false;
    }
    bPuzzleDoorUnlocked = true;
    IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, false);
    UpdateStatusLights();
    if (!bButtonsUnlocked)
    {
        for (AMazePuzzleButton* Button : SpawnedButtons)
        {
            if (IsValid(Button))
            {
                Button->SetPressed(false);
            }
        }
    }
    return false;
}

void UMazePuzzleComponent::OnCompletionAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* Player = Cast<APawn>(OtherActor);
    if (!bPrepared || bPuzzleDoorSealed || !IsValid(OverlappedComponent) || OverlappedComponent != CompletedExitArea.Get()
        || OverlappedComponent->GetOwner() != GetOwner() || !IsValid(Player) || !Player->IsPlayerControlled())
    {
        return;
    }
    CompletedExitPlayer = Player;
    bObservedPlayerInPuzzleArea = true;
    QueueCompletedExitCheck();
}

void UMazePuzzleComponent::OnCompletionAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (IsValid(OverlappedComponent) && OverlappedComponent == CompletedExitArea.Get()
        && OverlappedComponent->GetOwner() == GetOwner() && OtherActor == CompletedExitPlayer.Get())
    {
        // Wait a frame so multi-component overlap removal can finish first.
        QueueCompletedExitCheck();
    }
}

void UMazePuzzleComponent::QueueCompletedExitCheck()
{
    if (UWorld* World = GetWorld())
    {
        if (bPuzzleCompleted && !bPuzzleDoorSealed && bObservedPlayerInPuzzleArea && CompletedExitArea.IsValid()
            && !World->GetTimerManager().IsTimerActive(CompletedExitTimer))
        {
            World->GetTimerManager().SetTimer(CompletedExitTimer, this, &UMazePuzzleComponent::CheckCompletedPuzzleExit, 0.05f, false);
        }
    }
}

void UMazePuzzleComponent::CheckCompletedPuzzleExit()
{
    UBoxComponent* Area = CompletedExitArea.Get();
    AMazeActors* Room = Cast<AMazeActors>(GetOwner());
    if (!bArmed || !bPuzzleCompleted || bPuzzleDoorSealed || !bObservedPlayerInPuzzleArea || !IsValid(Room)
        || !IsValid(Area) || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze
        || !MazeGenerator->IsDoorUnlocked(Room, PuzzleDoorTag))
    {
        ClearCompletedExitArea();
        return;
    }
    // A disabled trigger must never masquerade as the player leaving the room.
    if (!IsRoomAreaEnabled(*Area))
    {
        ClearCompletedExitArea();
        return;
    }
    APawn* Player = CompletedExitPlayer.Get();
    if (!IsValid(Player) || !Player->IsPlayerControlled())
    {
        CompletedExitPlayer.Reset();
        bObservedPlayerInPuzzleArea = false;
        return;
    }
    // Any remaining player collision overlap means the doorway is not clear yet.
    if (Area->IsOverlappingActor(Player))
    {
        QueueCompletedExitCheck();
        return;
    }

    bPuzzleDoorSealed = true;
    bPuzzleDoorUnlocked = false;
    ClearCompletedExitArea();
    IMazeInterface::Execute_SetMazeDoorLocked(Room, PuzzleDoorTag, true);
    UpdateStatusLights();
}

void UMazePuzzleComponent::ClearSpawnedActors()
{
    for (AMazePuzzleButton* Button : SpawnedButtons)
    {
        if (IsValid(Button))
        {
            Button->Destroy();
        }
    }

    for (AMazePuzzleButton* Button : DebugButtons)
    {
        if (IsValid(Button))
        {
            Button->Destroy();
        }
    }

    ClearSpawnedWeepingAngels();
    SpawnedButtons.Reset();
    DebugButtons.Reset();
}

void UMazePuzzleComponent::ClearSpawnedWeepingAngels()
{
    for (APawn* Enemy : SpawnedWeepingAngels)
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

    SpawnedWeepingAngels.Reset();
}

void UMazePuzzleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPipeTrap();
    ClearPipePreviewObjects();
    ClearRuntimePipeMeshes();
    bArmed = false;
    SetComponentTickEnabled(false);
    ClearEnemyEntryTrigger();
    ClearCompletedExitArea();
    ClearPipeFailureTallies();
    StatusLightMaterials.Reset();
    StatusLightMeshes.Reset();
    ClearSpawnedActors();
    Super::EndPlay(EndPlayReason);
}

bool UMazePuzzleComponent::BuildPipeFrame(bool bRightWall, FMazePipeGridFrame& OutFrame, FString& OutError) const
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        OutError = TEXT("The pipe puzzle component needs a room owner.");
        return false;
    }

    const TArray<FName> Tags = { LeftBottomLeftTag, LeftBottomRightTag, LeftTopLeftTag, LeftTopRightTag,
        RightBottomLeftTag, RightBottomRightTag, RightTopLeftTag, RightTopRightTag };
    TArray<UArrowComponent*> Arrows;
    Owner->GetComponents<UArrowComponent>(Arrows);
    TArray<UArrowComponent*> UsedArrows;
    TArray<FName> UsedTags;
    TArray<FVector> Corners;
    for (FName Tag : Tags)
    {
        if (Tag.IsNone() || UsedTags.Contains(Tag))
        {
            OutError = TEXT("Each pipe corner needs a different nonempty component tag.");
            return false;
        }
        UsedTags.Add(Tag);

        UArrowComponent* Found = nullptr;
        for (UArrowComponent* Arrow : Arrows)
        {
            if (IsValid(Arrow) && Arrow->ComponentHasTag(Tag))
            {
                if (Found)
                {
                    OutError = FString::Printf(TEXT("More than one arrow has the pipe corner tag %s."), *Tag.ToString());
                    return false;
                }
                Found = Arrow;
            }
        }
        if (!Found || UsedArrows.Contains(Found))
        {
            OutError = FString::Printf(TEXT("Provide one separate arrow with component tag %s. Naming the component alone does not add its tag."), *Tag.ToString());
            return false;
        }
        UsedArrows.Add(Found);
        Corners.Add(Found->GetComponentLocation());
    }

    const FVector LeftCenter = (Corners[0] + Corners[1] + Corners[2] + Corners[3]) * 0.25;
    const FVector RightCenter = (Corners[4] + Corners[5] + Corners[6] + Corners[7]) * 0.25;
    const int32 Start = bRightWall ? 4 : 0;
    const TArray<FVector> WallCorners = { Corners[Start], Corners[Start + 1], Corners[Start + 2], Corners[Start + 3] };
    return FMazePipePuzzle::BuildGrid(WallCorners, bRightWall ? LeftCenter - RightCenter : RightCenter - LeftCenter,
        PipeGridSettings.Columns, PipeGridSettings.Rows, PipeBorderPadding, PipeWallOffset, OutFrame, OutError, PipeGridSettings.bFitColumnsToWall);
}

bool UMazePuzzleComponent::BuildPipeLayouts(int32 Seed, bool bHard)
{
    if (!bUsePipePuzzle)
    {
        return false;
    }

    bLeftPipeActive = false;
    bRightPipeActive = false;
    LeftPipeLayout = FMazePipeLayout();
    RightPipeLayout = FMazePipeLayout();
    FMazePipeGridFrame LeftFrame;
    FMazePipeGridFrame RightFrame;
    FString Error;
    if (!BuildPipeFrame(false, LeftFrame, Error) || !BuildPipeFrame(true, RightFrame, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: %s"), *Error);
        return false;
    }

    FRandomStream Selection(Seed);
    const bool bChooseRight = Selection.RandRange(0, 1) != 0;
    const int32 LeftSeed = static_cast<int32>(Selection.GetUnsignedInt());
    const int32 RightSeed = static_cast<int32>(Selection.GetUnsignedInt());
    const bool bUseLeft = bHard || !bChooseRight;
    const bool bUseRight = bHard || bChooseRight;
    FMazePipeLayout NewLeft;
    FMazePipeLayout NewRight;
    FMazePipeGridSettings LeftSettings = PipeGridSettings;
    FMazePipeGridSettings RightSettings = PipeGridSettings;
    LeftSettings.Columns = LeftFrame.Columns;
    LeftSettings.Rows = LeftFrame.Rows;
    RightSettings.Columns = RightFrame.Columns;
    RightSettings.Rows = RightFrame.Rows;
    if ((bUseLeft && !FMazePipePuzzle::Generate(LeftSettings, LeftSeed, NewLeft, Error))
        || (bUseRight && !FMazePipePuzzle::Generate(RightSettings, RightSeed, NewRight, Error)))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: %s"), *Error);
        return false;
    }

    LeftPipeLayout = MoveTemp(NewLeft);
    RightPipeLayout = MoveTemp(NewRight);
    bLeftPipeActive = bUseLeft;
    bRightPipeActive = bUseRight;
    return true;
}

bool UMazePuzzleComponent::GeneratePipeLayouts(int32 Seed)
{
    if (bPipeTrapTriggered || PipeTrap.IsActive())
    {
        return false;
    }
    ResetPipeCompletion();
    ResetPipeLights();
    const UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
    const bool bHard = !IsValid(SaveSystem) || SaveSystem->GetDifficulty() == EMazeDifficulty::Hard;
    const bool bGenerated = BuildPipeLayouts(Seed, bHard);
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        if (!bGenerated)
        {
            ClearRuntimePipeMeshes();
            return false;
        }
        return BuildPipeMeshes();
    }
    return bGenerated;
}

bool UMazePuzzleComponent::CanRotatePipeCell(bool bRightWall, int32 CellIndex) const
{
    if (!bUsePipePuzzle || bPipeSteamChecking || bPipeTrapTriggered || bPuzzleCompleted || (bRightWall ? bRightPipeCompleted : bLeftPipeCompleted)
        || !(bRightWall ? bRightPipeActive : bLeftPipeActive))
    {
        return false;
    }
    const FMazePipeLayout& Layout = bRightWall ? RightPipeLayout : LeftPipeLayout;
    if (!Layout.Cells.IsValidIndex(CellIndex) || Layout.Cells[CellIndex].bLocked
        || Layout.Cells[CellIndex].Piece == EMazePipePiece::Empty
        || Layout.Cells[CellIndex].Piece == EMazePipePiece::Inlet || Layout.Cells[CellIndex].Piece == EMazePipePiece::Outlet)
    {
        return false;
    }
    FMazePipeGridFrame Frame;
    FString Error;
    return BuildPipeFrame(bRightWall, Frame, Error) && Layout.Columns == Frame.Columns && Layout.Rows == Frame.Rows
        && Layout.Cells.Num() == Frame.Columns * Frame.Rows;
}

bool UMazePuzzleComponent::RotatePipeCell(bool bRightWall, int32 CellIndex, bool bRotateRight)
{
    if (!CanRotatePipeCell(bRightWall, CellIndex))
    {
        return false;
    }
    FMazePipeLayout& Layout = bRightWall ? RightPipeLayout : LeftPipeLayout;
    FMazePipeGridFrame Frame;
    FString Error;
    if (!BuildPipeFrame(bRightWall, Frame, Error))
    {
        return false;
    }
    if (!FMazePipePuzzle::RotateCell(Layout, CellIndex, bRotateRight))
    {
        return false;
    }
    ResetPipeLights();
    for (AMazePipePiece* Actor : PipeMeshActors)
    {
        if (IsValid(Actor) && Actor->bRightWall == bRightWall && Actor->GridCellIndex == CellIndex)
        {
            FVector X;
            FVector Y;
            FMazePipeFitting::GetCellAxes(Frame, Layout.Cells[CellIndex].Rotation, false, X, Y);
            Actor->SetActorRotation(FRotationMatrix::MakeFromXY(X, Y).ToQuat());
            Actor->ConfigureIndicators(Layout.Cells[CellIndex], Frame);
        }
    }
    UWorld* World = GetWorld();
    if (IsValid(PipeTurnSound) && IsValid(World) && World->IsGameWorld())
    {
        const float PitchMultiplier = FMath::FRandRange(0.90f, 1.10f);
        UGameplayStatics::PlaySoundAtLocation(this, PipeTurnSound.Get(), Frame.GetCellCenter(CellIndex),
            1.0f, PitchMultiplier);
    }
    return true;
}

FMazePipeSteamResult UMazePuzzleComponent::TracePipeWall(bool bRightWall) const
{
    if (!bUsePipePuzzle || !(bRightWall ? bRightPipeActive : bLeftPipeActive))
    {
        return FMazePipeSteamResult();
    }
    return FMazePipePuzzle::TraceSteam(bRightWall ? RightPipeLayout : LeftPipeLayout);
}

bool UMazePuzzleComponent::GetPipeCellLocation(bool bRightWall, int32 CellIndex, FVector& Location, float& CellSize) const
{
    Location = FVector::ZeroVector;
    CellSize = 0.0f;
    const FMazePipeLayout& Layout = bRightWall ? RightPipeLayout : LeftPipeLayout;
    if (!bUsePipePuzzle || !(bRightWall ? bRightPipeActive : bLeftPipeActive) || !Layout.Cells.IsValidIndex(CellIndex))
    {
        return false;
    }
    FMazePipeGridFrame Frame;
    FString Error;
    if (!BuildPipeFrame(bRightWall, Frame, Error) || Layout.Columns != Frame.Columns || Layout.Rows != Frame.Rows)
    {
        return false;
    }
    Location = Frame.GetCellCenter(CellIndex);
    CellSize = Frame.CellSize;
    return true;
}

void UMazePuzzleComponent::PreviewPipeGrid()
{
    if (!IsValid(GetWorld()) || GetWorld()->IsGameWorld())
    {
        return;
    }
    ClearPreview();
    DrawPipePreview(false, false);
}

void UMazePuzzleComponent::PreviewPipeLayout()
{
    if (!IsValid(GetWorld()) || GetWorld()->IsGameWorld())
    {
        return;
    }
    ClearPreview();
    // Preview both walls for authoring. Runtime still reads the existing save difficulty.
    if (BuildPipeLayouts(PipePreviewSeed, true))
    {
        DrawPipePreview(true, false);
    }
}

void UMazePuzzleComponent::PreviewPipeSolution()
{
    if (!IsValid(GetWorld()) || GetWorld()->IsGameWorld())
    {
        return;
    }
    ClearPreview();
    if (BuildPipeLayouts(PipePreviewSeed, true))
    {
        DrawPipePreview(true, true);
    }
}

void UMazePuzzleComponent::DrawPipePreview(bool bShowLayout, bool bShowSolution)
{
    UWorld* World = GetWorld();
    if (!bUsePipePuzzle || !IsValid(World) || World->IsGameWorld())
    {
        return;
    }
    ULineBatchComponent* Lines = GetPipePreviewLines();
    if (!IsValid(Lines))
    {
        return;
    }
    const float Duration = FMath::Clamp(PipePreviewDuration, 1.0f, 60.0f);
    for (int32 Wall = 0; Wall < 2; ++Wall)
    {
        FMazePipeGridFrame Frame;
        FString Error;
        if (!BuildPipeFrame(Wall != 0, Frame, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: %s"), *Error);
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("Pipe puzzle: %s wall has %d columns, %d rows and %.1f cm square cells."),
            Wall != 0 ? TEXT("Right") : TEXT("Left"), Frame.Columns, Frame.Rows, Frame.CellSize);
        for (int32 Column = 0; Column <= Frame.Columns; ++Column)
        {
            const FVector Start = Frame.BottomLeft + Frame.Right * (Column * Frame.CellSize);
            Lines->DrawLine(Start, Start + Frame.Up * (Frame.Rows * Frame.CellSize), FLinearColor(FColor(70, 100, 130)), 0, 1.0f, Duration, 0);
        }
        for (int32 Row = 0; Row <= Frame.Rows; ++Row)
        {
            const FVector Start = Frame.BottomLeft + Frame.Up * (Row * Frame.CellSize);
            Lines->DrawLine(Start, Start + Frame.Right * (Frame.Columns * Frame.CellSize), FLinearColor(FColor(70, 100, 130)), 0, 1.0f, Duration, 0);
        }

        const FMazePipeLayout& Layout = Wall != 0 ? RightPipeLayout : LeftPipeLayout;
        if (!bShowLayout || Layout.Columns != Frame.Columns || Layout.Rows != Frame.Rows)
        {
            continue;
        }
        for (int32 Index = 0; Index < Layout.Cells.Num(); ++Index)
        {
            FMazePipeCell Cell = Layout.Cells[Index];
            if (bShowSolution)
            {
                Cell.Rotation = Cell.SolutionRotation;
            }
            const FVector Center = Frame.GetCellCenter(Index) + Frame.Inward * 1.0f;
            const FColor Color = Cell.Piece == EMazePipePiece::Inlet ? FColor::Blue
                : Cell.Piece == EMazePipePiece::Outlet ? FColor(170, 60, 255)
                : Cell.Piece == EMazePipePiece::Chamber ? FColor::Yellow
                : bShowSolution && Cell.bOnSolution ? FColor::Cyan : FColor::White;
            const uint8 Ports = FMazePipePuzzle::GetPorts(Cell);
            const FVector Directions[] = { Frame.Up, Frame.Right, -Frame.Up, -Frame.Right };
            Lines->DrawPoint(Center, FLinearColor(Color), Cell.bLocked ? 14.0f : 7.0f, 0, Duration, 0);
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                if (Ports & (1 << Direction))
                {
                    Lines->DrawLine(Center, Center + Directions[Direction] * (Frame.CellSize * 0.5f), FLinearColor(Color), 0, 3.0f, Duration, 0);
                }
            }
            if (Cell.bLocked)
            {
                Lines->DrawLine(Center, Center - Frame.Inward * (Frame.CellSize * 0.25f), FLinearColor(Color), 0, 3.0f, Duration, 0);
            }
        }
    }
}

TSubclassOf<AMazePipePiece> UMazePuzzleComponent::GetPipeMeshClass(EMazePipePiece Piece) const
{
    switch (Piece)
    {
    case EMazePipePiece::Straight:
        return StraightPipeClass;
    case EMazePipePiece::Bend:
    case EMazePipePiece::Inlet:
    case EMazePipePiece::Outlet:
        return BendPipeClass;
    case EMazePipePiece::Tee:
        return TeePipeClass;
    case EMazePipePiece::Chamber:
        return ChamberPipeClass;
    case EMazePipePiece::Cross:
        return CrossPipeClass;
    default:
        return nullptr;
    }
}

USceneComponent* UMazePuzzleComponent::GetPipeWallParent(bool bRightWall) const
{
    if (!IsValid(GetOwner()))
    {
        return nullptr;
    }
    if (bUsePipePuzzle && PipeTrapSettings.bEnabled)
    {
        // The grid arrows are geometry markers; they may stay below DefaultSceneRoot.
        // Attach generated pipes to the same explicitly tagged roots as the spikes.
        const FName RootTag = bRightWall ? PipeTrapSettings.RightWallRootTag : PipeTrapSettings.LeftWallRootTag;
        if (RootTag.IsNone()) { return nullptr; }
        TArray<USceneComponent*> Components;
        GetOwner()->GetComponents<USceneComponent>(Components);
        USceneComponent* Found = nullptr;
        for (USceneComponent* Component : Components)
        {
            if (IsValid(Component) && Component->ComponentHasTag(RootTag))
            {
                if (Found) { return nullptr; }
                Found = Component;
            }
        }
        return Found == GetOwner()->GetRootComponent() ? nullptr : Found;
    }

    TArray<UArrowComponent*> Arrows;
    GetOwner()->GetComponents<UArrowComponent>(Arrows);
    const FName Tag = bRightWall ? RightBottomLeftTag : LeftBottomLeftTag;
    for (UArrowComponent* Arrow : Arrows)
    {
        if (IsValid(Arrow) && Arrow->ComponentHasTag(Tag))
        {
            return IsValid(Arrow->GetAttachParent()) ? Arrow->GetAttachParent() : GetOwner()->GetRootComponent();
        }
    }
    return GetOwner()->GetRootComponent();
}

bool UMazePuzzleComponent::SpawnPipePreviewPiece(bool bRightWall, const FMazePipeGridFrame& Frame, int32 CellIndex, const FMazePipeCell& Cell)
{
    return SpawnPipePiece(bRightWall, Frame, CellIndex, Cell, true);
}

bool UMazePuzzleComponent::SpawnPipePiece(bool bRightWall, const FMazePipeGridFrame& Frame, int32 CellIndex, const FMazePipeCell& Cell, bool bPreview)
{
    const TSubclassOf<AMazePipePiece> Class = GetPipeMeshClass(Cell.Piece);
    if (!Class || !StraightPipeClass || !IsValid(GetWorld()) || !IsValid(GetOwner()))
    {
        return false;
    }
    const AMazePipePiece* StraightSource = StraightPipeClass->GetDefaultObject<AMazePipePiece>();
    const bool bWallConnection = Cell.Piece == EMazePipePiece::Inlet || Cell.Piece == EMazePipePiece::Outlet;
    FVector X;
    FVector Y;
    FMazePipeFitting::GetCellAxes(Frame, Cell.Rotation, bWallConnection, X, Y);
    const FTransform Transform(FRotationMatrix::MakeFromXY(X, Y).ToQuat(), Frame.GetCellCenter(CellIndex));
    FActorSpawnParameters Spawn;
    Spawn.Owner = GetOwner();
    Spawn.ObjectFlags |= RF_Transient;
    if (bPreview)
    {
        Spawn.ObjectFlags |= RF_DuplicateTransient;
    }
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AMazePipePiece* Actor = GetWorld()->SpawnActor<AMazePipePiece>(Class, Transform, Spawn);
    if (!IsValid(Actor))
    {
        return false;
    }
    FString Error;
    if (!Actor->FitToCell(Cell, Frame.CellSize, Frame.CellSize * PipeDiameterFraction,
        PipeWallOffset + PipeWallEmbedDepth, StraightSource, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe mesh: %s: %s"), *Class->GetName(), *Error);
        Actor->Destroy();
        return false;
    }
    if (bPreview)
    {
        Actor->Tags.Add(FName(TEXT("MazePipeMeshPreview")));
    }
    Actor->GridCellIndex = CellIndex;
    Actor->bRightWall = bRightWall;
    Actor->ConfigureIndicators(Cell, Frame);
    Actor->InitializeInteraction(this, !bPreview);
    USceneComponent* Parent = GetPipeWallParent(bRightWall);
    if (bUsePipePuzzle && PipeTrapSettings.bEnabled)
    {
        Parent = PipeTrap.FindPipePanelParent(GetOwner(), GetResolvedPipeTrapSettings(), bRightWall, Frame,
            PipeWallOffset, Frame.GetCellCenter(CellIndex), Error);
        if (!IsValid(Parent))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe panel attachment: %s"), *Error);
            Actor->Destroy();
            return false;
        }
    }
    if (IsValid(Parent))
    {
        Actor->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
    }
    if (bPreview)
    {
        PipeMeshPreviewActors.Add(Actor);
    }
    else
    {
        PipeMeshActors.Add(Actor);
    }
    return true;
}

void UMazePuzzleComponent::ClearPipeMeshPreview()
{
    ClearPreview();
}

void UMazePuzzleComponent::ClearPreview()
{
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        return;
    }
    ClearPipePreviewObjects();
}

void UMazePuzzleComponent::ClearPipePreviewObjects()
{
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        PipeTrap.ClearSpikes();
    }
    ResetPipeCompletion();
    ResetPipeLights();
    bHasPipeLayoutPreview = false;
    bPipePreviewShowsSolution = false;
    for (AMazePipePiece* Actor : PipeMeshPreviewActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    PipeMeshPreviewActors.Reset();
    if (IsValid(PipePreviewLines))
    {
        PipePreviewLines->Flush();
        PipePreviewLines->DestroyComponent();
        PipePreviewLines = nullptr;
    }
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        bLeftPipeActive = false;
        bRightPipeActive = false;
        LeftPipeLayout = FMazePipeLayout();
        RightPipeLayout = FMazePipeLayout();
        bPipeCheckUsesPreview = false;
    }
}

ULineBatchComponent* UMazePuzzleComponent::GetPipePreviewLines()
{
    if (!IsValid(GetWorld()) || GetWorld()->IsGameWorld() || !IsValid(GetOwner()))
    {
        return nullptr;
    }
    if (!IsValid(PipePreviewLines))
    {
        PipePreviewLines = NewObject<ULineBatchComponent>(GetOwner(), NAME_None, RF_Transient | RF_DuplicateTransient);
        PipePreviewLines->SetMobility(EComponentMobility::Movable);
        PipePreviewLines->SetWorldTransform(FTransform::Identity);
        PipePreviewLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PipePreviewLines->SetCanEverAffectNavigation(false);
        PipePreviewLines->bIsEditorOnly = true;
        PipePreviewLines->bCalculateAccurateBounds = true;
#if WITH_EDITOR
        PipePreviewLines->bTickInEditor = true;
#endif
        GetOwner()->AddInstanceComponent(PipePreviewLines.Get());
        PipePreviewLines->RegisterComponentWithWorld(GetWorld());
        PipePreviewLines->SetComponentTickEnabled(true);
    }
    return PipePreviewLines.Get();
}

void UMazePuzzleComponent::ResetPipeCompletion()
{
    bLeftPipeCompleted = false;
    bRightPipeCompleted = false;
    LeftPipeSteamResult = FMazePipeSteamResult();
    RightPipeSteamResult = FMazePipeSteamResult();
    CheckedLeftPipeLayout = FMazePipeLayout();
    CheckedRightPipeLayout = FMazePipeLayout();
}

void UMazePuzzleComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ClearPipeTrap();
    ClearPipePreviewObjects();
    ClearRuntimePipeMeshes();
    ClearPipeFailureTallies();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UMazePuzzleComponent::PreviewPipeMeshFit()
{
    PreviewFailedPipeMesh();
}

void UMazePuzzleComponent::PreviewFailedPipeMesh()
{
    if (!bUsePipePuzzle || !IsValid(GetWorld()) || GetWorld()->IsGameWorld())
    {
        return;
    }
    BuildPipeMeshPreview(false, true);
    if (bHasPipeLayoutPreview)
    {
        CheckPipePuzzle();
    }
}

void UMazePuzzleComponent::BuildPipeMeshPreview(bool bShowSolution, bool bForceFailure)
{
    if (!bUsePipePuzzle || !IsValid(GetWorld()) || GetWorld()->IsGameWorld())
    {
        return;
    }
    if (!StraightPipeClass || !BendPipeClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe mesh preview: Assign the Straight Pipe Class and Bend Pipe Class first."));
        return;
    }
    ClearPreview();
    if (!BuildPipeLayouts(PipePreviewSeed, true))
    {
        return;
    }
    if (bForceFailure)
    {
        for (int32 Wall = 0; Wall < 2; ++Wall)
        {
            FMazePipeLayout& Layout = Wall != 0 ? RightPipeLayout : LeftPipeLayout;
            FMazePipeLayout Failed;
            FString Error;
            int32 ChangedCell = INDEX_NONE;
            if (!FMazePipePuzzle::MakeFailedPreview(Layout, Failed, ChangedCell, Error))
            {
                UE_LOG(LogTemp, Error, TEXT("Failed pipe preview: %s"), *Error);
                return;
            }
            Layout = MoveTemp(Failed);
            UE_LOG(LogTemp, Display, TEXT("Failed pipe preview: %s wall has one incorrect piece at cell %d."),
                Wall != 0 ? TEXT("Right") : TEXT("Left"), ChangedCell);
        }
    }
    WarnPipeLightMaterials();
    DrawPipePreview(false, false);
    int32 MissingClasses = 0;
    ULineBatchComponent* Lines = GetPipePreviewLines();
    if (!IsValid(Lines))
    {
        return;
    }
    const float Duration = FMath::Clamp(PipePreviewDuration, 1.0f, 60.0f);
    for (int32 Wall = 0; Wall < 2; ++Wall)
    {
        FMazePipeGridFrame Frame;
        FString Error;
        if (!BuildPipeFrame(Wall != 0, Frame, Error))
        {
            ClearPreview();
            return;
        }
        const FMazePipeLayout& Layout = Wall != 0 ? RightPipeLayout : LeftPipeLayout;
        for (int32 Index = 0; Index < Layout.Cells.Num(); ++Index)
        {
            FMazePipeCell Cell = Layout.Cells[Index];
            if (bShowSolution && !bForceFailure)
            {
                Cell.Rotation = Cell.SolutionRotation;
            }
            if (GetPipeMeshClass(Cell.Piece))
            {
                if (!SpawnPipePreviewPiece(Wall != 0, Frame, Index, Cell))
                {
                    ClearPreview();
                    return;
                }
            }
            else
            {
                ++MissingClasses;
                const FVector Center = Frame.GetCellCenter(Index);
                const FVector Directions[] = { Frame.Up, Frame.Right, -Frame.Up, -Frame.Right };
                const uint8 Ports = FMazePipePuzzle::GetPorts(Cell);
                const FColor Color = Cell.Piece == EMazePipePiece::Chamber ? FColor::Yellow : FColor::White;
                for (int32 Direction = 0; Direction < 4; ++Direction)
                {
                    if (Ports & (1 << Direction))
                    {
                        Lines->DrawLine(Center, Center + Directions[Direction] * (Frame.CellSize * 0.5f), FLinearColor(Color), 0, 3.0f, Duration, 0);
                    }
                }
            }
        }
    }
    if (MissingClasses > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Pipe mesh preview: %d pieces still use lines. Assign the remaining Tee, Chamber and Cross classes when their source meshes are fitted."), MissingClasses);
    }
    bHasPipeLayoutPreview = true;
    bPipePreviewShowsSolution = bShowSolution && !bForceFailure;
}

void UMazePuzzleComponent::PreviewPipeMeshes()
{
    BuildPipeMeshPreview(false);
}

void UMazePuzzleComponent::PreviewSolvedPipeMeshes()
{
    BuildPipeMeshPreview(true);
}

void UMazePuzzleComponent::WarnPipeLightMaterials() const
{
    const TArray<TSubclassOf<AMazePipePiece>> Classes = { StraightPipeClass, BendPipeClass, TeePipeClass, ChamberPipeClass, CrossPipeClass };
    for (const TSubclassOf<AMazePipePiece>& Class : Classes)
    {
        if (Class)
        {
            const AMazePipePiece* Source = Class->GetDefaultObject<AMazePipePiece>();
            if (Source->bGlowWholePipe && !IsValid(Source->IndicatorMaterial))
            {
                UE_LOG(LogTemp, Warning, TEXT("Pipe glow: Assign Indicator Material in the Class Defaults of %s."), *Class->GetName());
            }
        }
    }
}

void UMazePuzzleComponent::ClearRuntimePipeMeshes()
{
    PipeTrap.ClearSpikes();
    ResetPipeCompletion();
    ResetPipeLights();
    for (AMazePipePiece* Actor : PipeMeshActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    PipeMeshActors.Reset();
}

bool UMazePuzzleComponent::BuildPipeMeshes()
{
    if (!bUsePipePuzzle || bPipeSteamChecking || bPipeTrapTriggered || !IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return false;
    }
    ClearRuntimePipeMeshes();
    if (!StraightPipeClass || !BendPipeClass || !TeePipeClass || !ChamberPipeClass || !CrossPipeClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Assign all five pipe mesh classes before generating runtime pipes."));
        return false;
    }
    if (!bLeftPipeActive && !bRightPipeActive)
    {
        return false;
    }
    WarnPipeLightMaterials();
    for (int32 Wall = 0; Wall < 2; ++Wall)
    {
        const bool bRightWall = Wall != 0;
        if (!(bRightWall ? bRightPipeActive : bLeftPipeActive))
        {
            continue;
        }
        FMazePipeGridFrame Frame;
        FString Error;
        const FMazePipeLayout& Layout = bRightWall ? RightPipeLayout : LeftPipeLayout;
        if (!BuildPipeFrame(bRightWall, Frame, Error) || Layout.Columns != Frame.Columns || Layout.Rows != Frame.Rows
            || Layout.Cells.Num() != Frame.Columns * Frame.Rows)
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Regenerate the layout after changing the grid. %s"), *Error);
            ClearRuntimePipeMeshes();
            return false;
        }
        for (int32 Index = 0; Index < Layout.Cells.Num(); ++Index)
        {
            if (!SpawnPipePiece(bRightWall, Frame, Index, Layout.Cells[Index], false))
            {
                ClearRuntimePipeMeshes();
                return false;
            }
        }
    }
    if (!BuildPipeTrapSpikes(false))
    {
        ClearRuntimePipeMeshes();
        return false;
    }
    return true;
}

void UMazePuzzleComponent::PreviewPipeSteamCheck()
{
    if (IsValid(GetWorld()) && !GetWorld()->IsGameWorld())
    {
        CheckPipePuzzle();
    }
}

bool UMazePuzzleComponent::CheckPipePuzzle()
{
    if (!bUsePipePuzzle || bPipeSteamChecking || bPipeTrapTriggered || bPuzzleCompleted || !IsValid(GetWorld()))
    {
        return false;
    }
    if (!StraightPipeClass || !BendPipeClass || !TeePipeClass || !ChamberPipeClass || !CrossPipeClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Assign all five pipe classes before checking the pipe lights."));
        return false;
    }
    const bool bPreview = !GetWorld()->IsGameWorld();
    if (bPreview && !bHasPipeLayoutPreview)
    {
        BuildPipeMeshPreview(false);
        if (!bHasPipeLayoutPreview)
        {
            return false;
        }
    }
    if ((!bLeftPipeActive && !bRightPipeActive)
        || ((!bLeftPipeActive || bLeftPipeCompleted) && (!bRightPipeActive || bRightPipeCompleted)))
    {
        return false;
    }
    const int32 ExpectedActors = (bLeftPipeActive ? LeftPipeLayout.Cells.Num() : 0)
        + (bRightPipeActive ? RightPipeLayout.Cells.Num() : 0);
    const TArray<TObjectPtr<AMazePipePiece>>& Actors = bPreview ? PipeMeshPreviewActors : PipeMeshActors;
    if (Actors.Num() != ExpectedActors || Actors.ContainsByPredicate([](const AMazePipePiece* Actor) { return !IsValid(Actor); }))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Rebuild the full pipe mesh board before checking it."));
        return false;
    }
    if (!FMath::IsFinite(PipeSteamStepSeconds) || PipeSteamStepSeconds < 0.02f
        || !FMath::IsFinite(PipeSteamHoldSeconds) || PipeSteamHoldSeconds < 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Use a positive steam step duration and a nonnegative result hold duration."));
        return false;
    }
    ResetPipeLights();
    CheckedLeftPipeLayout = LeftPipeLayout;
    CheckedRightPipeLayout = RightPipeLayout;
    if (bPreview && bPipePreviewShowsSolution)
    {
        FMazePipePuzzle::RestoreSolution(CheckedLeftPipeLayout);
        FMazePipePuzzle::RestoreSolution(CheckedRightPipeLayout);
    }
    if (!bLeftPipeCompleted)
    {
        LeftPipeSteamResult = bLeftPipeActive ? FMazePipePuzzle::TraceSteam(CheckedLeftPipeLayout) : FMazePipeSteamResult();
    }
    if (!bRightPipeCompleted)
    {
        RightPipeSteamResult = bRightPipeActive ? FMazePipePuzzle::TraceSteam(CheckedRightPipeLayout) : FMazePipeSteamResult();
    }
    if ((bLeftPipeActive && LeftPipeSteamResult.SteamDistances.Num() != CheckedLeftPipeLayout.Cells.Num())
        || (bRightPipeActive && RightPipeSteamResult.SteamDistances.Num() != CheckedRightPipeLayout.Cells.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Regenerate invalid pipe data before starting a steam check."));
        return false;
    }
    LastPipeSteamStep = FMath::Max(bLeftPipeCompleted ? INDEX_NONE : FMazePipePuzzle::GetLastSteamLightStep(LeftPipeSteamResult),
        bRightPipeCompleted ? INDEX_NONE : FMazePipePuzzle::GetLastSteamLightStep(RightPipeSteamResult));
    if (LastPipeSteamStep == INDEX_NONE)
    {
        return false;
    }
    bPipeCheckUsesPreview = bPreview;
    bPipeCheckIsStandalone = IsStandalonePipeTest();
    bPipeFailureRecorded = false;
    bPipeSteamChecking = true;
    bPipeSteamResultShown = false;
    PipeSteamStep = 0;
    PipeSteamElapsed = 0.0f;
    PipeSteamHoldRemaining = 0.0f;
    ApplyPipeSteamStep();
    SetComponentTickEnabled(true);
    return true;
}

void UMazePuzzleComponent::ResetPipeLights()
{
    if (!bPipeTrapTriggered) { ReleasePipeTrapPlayer(); }
    ClearPipeLeakSteam();
    bPipeSteamChecking = false;
    bPipeSteamResultShown = false;
    bPipeCheckIsStandalone = false;
    PipeSteamElapsed = 0.0f;
    PipeSteamHoldRemaining = 0.0f;
    const auto ResetActors = [this](const TArray<TObjectPtr<AMazePipePiece>>& Actors)
        {
            for (AMazePipePiece* Actor : Actors)
            {
                if (!IsValid(Actor))
                {
                    continue;
                }
                const bool bCompleted = Actor->bRightWall ? bRightPipeCompleted : bLeftPipeCompleted;
                const FMazePipeSteamResult& Result = Actor->bRightWall ? RightPipeSteamResult : LeftPipeSteamResult;
                const FMazePipeLayout& Layout = Actor->bRightWall ? CheckedRightPipeLayout : CheckedLeftPipeLayout;
                const EMazePipeFlowLightState Flow = bCompleted
                    ? FMazePipePuzzle::GetSteamLightState(Result, Actor->GridCellIndex, 0, true) : EMazePipeFlowLightState::Idle;
                const bool bChamber = Layout.Cells.IsValidIndex(Actor->GridCellIndex)
                    && Layout.Cells[Actor->GridCellIndex].Piece == EMazePipePiece::Chamber;
                Actor->SetIndicatorStates(Flow, bCompleted && bChamber
                    ? EMazePipeChamberLightState::Connected : EMazePipeChamberLightState::Required);
            }
        };
    ResetActors(PipeMeshPreviewActors);
    ResetActors(PipeMeshActors);
    SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive() || bEnemySpawningPending
        || ShouldWatchRoomVisit() || (bArmed && bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked));
}

void UMazePuzzleComponent::ApplyPipeSteamStep()
{
    if (bPipeSteamChecking)
    {
        if (bLeftPipeActive)
        {
            bLeftPipeCompleted = FMazePipePuzzle::AdvanceSteamCompletion(LeftPipeSteamResult, PipeSteamStep, bLeftPipeCompleted);
        }
        if (bRightPipeActive)
        {
            bRightPipeCompleted = FMazePipePuzzle::AdvanceSteamCompletion(RightPipeSteamResult, PipeSteamStep, bRightPipeCompleted);
        }
    }
    const int32 LeftStep = bLeftPipeCompleted ? FMazePipePuzzle::GetLastSteamLightStep(LeftPipeSteamResult) : PipeSteamStep;
    const int32 RightStep = bRightPipeCompleted ? FMazePipePuzzle::GetLastSteamLightStep(RightPipeSteamResult) : PipeSteamStep;
    const bool bLeftFinished = FMazePipePuzzle::IsSteamCheckFinished(LeftPipeSteamResult, LeftStep);
    const bool bRightFinished = FMazePipePuzzle::IsSteamCheckFinished(RightPipeSteamResult, RightStep);
    const TArray<TObjectPtr<AMazePipePiece>>& Actors = bPipeCheckUsesPreview ? PipeMeshPreviewActors : PipeMeshActors;
    for (AMazePipePiece* Actor : Actors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }
        const FMazePipeLayout& Layout = Actor->bRightWall ? CheckedRightPipeLayout : CheckedLeftPipeLayout;
        const FMazePipeSteamResult& Result = Actor->bRightWall ? RightPipeSteamResult : LeftPipeSteamResult;
        const bool bCompleted = Actor->bRightWall ? bRightPipeCompleted : bLeftPipeCompleted;
        const int32 Step = Actor->bRightWall ? RightStep : LeftStep;
        const EMazePipeFlowLightState Flow = FMazePipePuzzle::GetSteamLightState(Result, Actor->GridCellIndex, Step, bCompleted);
        const EMazePipeChamberLightState Chamber = Layout.Cells.IsValidIndex(Actor->GridCellIndex)
            && Layout.Cells[Actor->GridCellIndex].Piece == EMazePipePiece::Chamber
            ? FMazePipePuzzle::GetChamberLightState(Layout, Result, Step, Actor->bRightWall ? bRightFinished : bLeftFinished)
            : EMazePipeChamberLightState::Required;
        if (Actor->FlowLightState != Flow || Actor->ChamberLightState != Chamber)
        {
            Actor->SetIndicatorStates(Flow, Chamber);
        }
    }
}

bool UMazePuzzleComponent::InitializePipeFailureTallies()
{
    ClearPipeFailureTallies();
    PipeFailedSteamChecks = 0;
    bPipeFailureRecorded = false;
    bPipeTrapTriggered = false;
    if (!bUsePipePuzzle)
    {
        return true;
    }
    AActor* Room = GetOwner();
    if (!IsValid(Room) || !GetWorld() || !GetWorld()->IsGameWorld() || PipeFailureTallyTag.IsNone()
        || PipeFailureTallyColorParameter.IsNone() || PipeFailureTallyMaterialIndex < 0)
    {
        return false;
    }

    TArray<UMeshComponent*> Meshes;
    TArray<UMeshComponent*> Tallies;
    const auto AddTally = [this, &Tallies](UMeshComponent* Mesh)
        {
            if (Mesh->ComponentHasTag(StatusLightComponentTag))
            {
                UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s cannot also use the doorway status tag."), *Mesh->GetName());
                return false;
            }
            Tallies.AddUnique(Mesh);
            return true;
        };
    Room->GetComponents<UMeshComponent>(Meshes, true);
    for (UMeshComponent* Mesh : Meshes)
    {
        if (IsValid(Mesh) && Mesh->ComponentHasTag(PipeFailureTallyTag) && !AddTally(Mesh))
        {
            return false;
        }
    }

    // A tally asset can carry its shared tag on the room's Child Actor Component.
    TArray<UChildActorComponent*> ChildComponents;
    TArray<FPipeTallyLightState> TallyLights;
    Room->GetComponents<UChildActorComponent>(ChildComponents, true);
    for (UChildActorComponent* ChildComponent : ChildComponents)
    {
        if (!IsValid(ChildComponent) || !ChildComponent->ComponentHasTag(PipeFailureTallyTag))
        {
            continue;
        }
        if (ChildComponent->ComponentHasTag(StatusLightComponentTag))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s cannot also use the doorway status tag."), *ChildComponent->GetName());
            return false;
        }
        AActor* Child = ChildComponent->GetChildActor();
        if (!IsValid(Child))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s in %s has no child actor. Assign its tally light Child Actor Class."),
                *ChildComponent->GetName(), *Room->GetPathName());
            return false;
        }
        TArray<UMeshComponent*> ChildMeshes;
        TArray<UMeshComponent*> Candidates;
        Child->GetComponents<UMeshComponent>(ChildMeshes, true);
        for (UMeshComponent* Mesh : ChildMeshes)
        {
            if (IsValid(Mesh) && Mesh->ComponentHasTag(PipeFailureTallyTag))
            {
                Candidates.AddUnique(Mesh);
            }
        }
        if (Candidates.IsEmpty())
        {
            for (UMeshComponent* Mesh : ChildMeshes)
            {
                if (IsValid(Mesh))
                {
                    Candidates.AddUnique(Mesh);
                }
            }
        }
        if (Candidates.Num() != 1)
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s in %s resolves to %d meshes. A tagged Child Actor Component needs one mesh, or exactly one inner mesh tagged %s."),
                *ChildComponent->GetName(), *Room->GetPathName(), Candidates.Num(), *PipeFailureTallyTag.ToString());
            return false;
        }
        if (!AddTally(Candidates[0]))
        {
            return false;
        }
        TArray<ULightComponent*> Lights;
        Child->GetComponents<ULightComponent>(Lights, true);
        for (ULightComponent* Light : Lights)
        {
            if (!IsValid(Light) || TallyLights.ContainsByPredicate([Light](const FPipeTallyLightState& State)
                { return State.Light.Get() == Light; }))
            {
                continue;
            }
            FPipeTallyLightState State;
            State.Mesh = Candidates[0];
            State.Light = Light;
            State.OriginalColor = Light->GetLightColor();
            State.bOriginalVisible = Light->IsVisible();
            TallyLights.Add(State);
        }
    }
    if (Tallies.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s has no meshes or Child Actor Components tagged %s. Save the tally Component Tags in the room Blueprint defaults."),
            *Room->GetPathName(), *PipeFailureTallyTag.ToString());
        return false;
    }

    // Use the widest local axis so a rotated room keeps the same visible tally order.
    const FTransform RoomTransform = Room->GetActorTransform();
    FBox LocalBounds(ForceInit);
    for (UMeshComponent* Mesh : Tallies)
    {
        const FVector Position = RoomTransform.InverseTransformPosition(Mesh->GetComponentLocation());
        if (Position.ContainsNaN())
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s has an invalid position."), *Mesh->GetName());
            return false;
        }
        LocalBounds += Position;
    }
    const FVector Size = LocalBounds.GetSize();
    int32 Axis = Size.Y > Size.X ? 1 : 0;
    if (Size.Z > Size[Axis])
    {
        Axis = 2;
    }
    Tallies.Sort([this, RoomTransform, Axis](const UMeshComponent& A, const UMeshComponent& B)
        {
            const double First = RoomTransform.InverseTransformPosition(A.GetComponentLocation())[Axis];
            const double Second = RoomTransform.InverseTransformPosition(B.GetComponentLocation())[Axis];
            if (First != Second)
            {
                return bReversePipeFailureTallies ? First > Second : First < Second;
            }
            return bReversePipeFailureTallies ? B.GetName() < A.GetName() : A.GetName() < B.GetName();
        });

    CachedPipeFailureTallyMaterialIndex = PipeFailureTallyMaterialIndex;
    CachedPipeFailureTallyColorParameter = PipeFailureTallyColorParameter;
    for (UMeshComponent* Mesh : Tallies)
    {
        if (CachedPipeFailureTallyMaterialIndex >= Mesh->GetNumMaterials())
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: %s has no material slot %d."), *Mesh->GetName(), CachedPipeFailureTallyMaterialIndex);
            ClearPipeFailureTallies();
            return false;
        }
        UMaterialInterface* Original = Mesh->GetMaterial(CachedPipeFailureTallyMaterialIndex);
        UMaterialInterface* Source = IsValid(PipeFailureTallyMaterial) ? PipeFailureTallyMaterial.Get() : Original;
        if (IsValid(Source))
        {
            TArray<FMaterialParameterInfo> Parameters;
            TArray<FGuid> ParameterIds;
            Source->GetAllVectorParameterInfo(Parameters, ParameterIds);
            if (!Parameters.ContainsByPredicate([this](const FMaterialParameterInfo& Parameter)
                { return Parameter.Name == CachedPipeFailureTallyColorParameter; }))
            {
                UE_LOG(LogTemp, Warning, TEXT("Pipe tally: Material on %s has no vector parameter %s. Assign a compatible Pipe Failure Tally Material to control the glowing face."),
                    *Mesh->GetName(), *CachedPipeFailureTallyColorParameter.ToString());
            }
        }
        UMaterialInterface* Parent = Source;
        while (UMaterialInstanceDynamic* DynamicParent = Cast<UMaterialInstanceDynamic>(Parent))
        {
            Parent = DynamicParent->Parent;
        }
        UMaterialInstanceDynamic* Material = IsValid(Parent) ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
        if (!IsValid(Material))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe tally: Assign a valid material for %s."), *Mesh->GetName());
            ClearPipeFailureTallies();
            return false;
        }
        Material->CopyMaterialUniformParameters(Source);
        Mesh->SetMaterial(CachedPipeFailureTallyMaterialIndex, Material);
        PipeFailureTallyMeshes.Add(Mesh);
        PipeFailureTallyMaterials.Add(Material);
        PipeFailureTallyOriginalMaterials.Add(Original);
    }
    PipeFailureTallyLights = TallyLights;
    PipeFailureTallyCount = Tallies.Num();
    UpdatePipeFailureTallies();
    return true;
}

void UMazePuzzleComponent::UpdatePipeFailureTallies()
{
    if (CachedPipeFailureTallyMaterialIndex < 0 || CachedPipeFailureTallyColorParameter.IsNone())
    {
        return;
    }
    for (int32 Index = 0; Index < PipeFailureTallyMaterials.Num(); ++Index)
    {
        UMaterialInstanceDynamic* Material = PipeFailureTallyMaterials[Index];
        if (!IsValid(Material) || !PipeFailureTallyMeshes.IsValidIndex(Index))
        {
            continue;
        }
        UMeshComponent* Mesh = PipeFailureTallyMeshes[Index].Get();
        if (!IsValid(Mesh))
        {
            continue;
        }
        if (Mesh->GetMaterial(CachedPipeFailureTallyMaterialIndex) != Material)
        {
            Mesh->SetMaterial(CachedPipeFailureTallyMaterialIndex, Material);
        }
        const bool bFailed = Index < PipeFailedSteamChecks;
        Material->SetVectorParameterValue(CachedPipeFailureTallyColorParameter,
            bFailed ? PipeFailureTallyFailedColor : PipeFailureTallyOffColor);
        for (const FPipeTallyLightState& State : PipeFailureTallyLights)
        {
            if (State.Mesh.Get() == Mesh && State.Light.IsValid())
            {
                State.Light->SetLightColor(PipeFailureTallyFailedColor);
                State.Light->SetVisibility(bFailed);
            }
        }
    }
}

void UMazePuzzleComponent::ClearPipeFailureTallies()
{
    ClearPipeLeakSteam();
    for (const FPipeTallyLightState& State : PipeFailureTallyLights)
    {
        if (State.Light.IsValid())
        {
            State.Light->SetLightColor(State.OriginalColor);
            State.Light->SetVisibility(State.bOriginalVisible);
        }
    }
    PipeFailureTallyLights.Reset();
    for (int32 Index = 0; Index < PipeFailureTallyMeshes.Num(); ++Index)
    {
        UMeshComponent* Mesh = PipeFailureTallyMeshes[Index].Get();
        if (IsValid(Mesh) && CachedPipeFailureTallyMaterialIndex >= 0 && PipeFailureTallyMaterials.IsValidIndex(Index)
            && PipeFailureTallyOriginalMaterials.IsValidIndex(Index)
            && Mesh->GetMaterial(CachedPipeFailureTallyMaterialIndex) == PipeFailureTallyMaterials[Index])
        {
            Mesh->SetMaterial(CachedPipeFailureTallyMaterialIndex, PipeFailureTallyOriginalMaterials[Index]);
        }
    }
    PipeFailureTallyMeshes.Reset();
    PipeFailureTallyMaterials.Reset();
    PipeFailureTallyOriginalMaterials.Reset();
    PipeFailureTallyCount = 0;
    CachedPipeFailureTallyMaterialIndex = INDEX_NONE;
    CachedPipeFailureTallyColorParameter = NAME_None;
}

FMazePipeTrapSettings UMazePuzzleComponent::GetResolvedPipeTrapSettings() const
{
    FMazePipeTrapSettings Settings = PipeTrapSettings;
    if (!IsValid(Settings.SpikeMesh))
    {
        Settings.SpikeMesh = DefaultPipeSpikeMesh;
    }
    if (!IsValid(Settings.SpikeMaterial))
    {
        Settings.SpikeMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
    }
    return Settings;
}

bool UMazePuzzleComponent::BuildPipeTrapSpikes(bool bPreview)
{
    const FMazePipeTrapSettings Settings = GetResolvedPipeTrapSettings();
    if (!bUsePipePuzzle || !Settings.bEnabled || !Settings.bGenerateSpikesFromGrid)
    {
        PipeTrap.ClearSpikes();
        return true;
    }
    FMazePipeGridFrame Left;
    FMazePipeGridFrame Right;
    FString Error;
    if (!BuildPipeFrame(false, Left, Error) || !BuildPipeFrame(true, Right, Error)
        || !PipeTrap.BuildSpikes(GetOwner(), Settings, Left, Right, GetPipeWallParent(false),
            GetPipeWallParent(true), PipeWallOffset, bPreview, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe spike grid: %s: %s"), *GetNameSafe(GetOwner()), *Error);
        return false;
    }
    return true;
}

void UMazePuzzleComponent::PreviewPipeSpikes()
{
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        BuildPipeTrapSpikes(true);
    }
}

bool UMazePuzzleComponent::SwitchPipeTrapWalls()
{
    if (!bUsePipePuzzle || !bPipeTrapTriggered || !PipeTrapSettings.bEnabled
        || !PipeTrapReveal.IsPlayerLocked() || !PipeTrapReveal.HasValidPlayer()
        || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
    }
    if (!PipeTrap.IsActive())
    {
        FMazePipeGridFrame Left;
        FMazePipeGridFrame Right;
        FString Error;
        if (!BuildPipeFrame(false, Left, Error) || !BuildPipeFrame(true, Right, Error)
            || !PipeTrap.Start(GetOwner(), GetResolvedPipeTrapSettings(), Left, Right,
                GetPipeWallParent(false), GetPipeWallParent(true), PipeWallOffset, Error, CompletedExitArea.Get()))
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe sequence wall switch: %s"), *Error);
            return false;
        }
    }
    const bool bSwitched = PipeTrap.SwitchToSpikes(PipeWallTurnSound.Get());
    if (bSwitched) { ClearPipeLeakSteam(); }
    SetComponentTickEnabled(true);
    return bSwitched;
}

bool UMazePuzzleComponent::UnlockPipeTrapPlayer()
{
    if (bPipeTrapSequenceFinished) { return true; }
    if (!bPipeTrapTriggered || !PipeTrapReveal.IsPlayerLocked() || !PipeTrapReveal.HasValidPlayer())
    {
        return false;
    }
    // Apply the final key now, even if Sequencer evaluates before this component's tick.
    if (bPipeTrapUseSequenceLook) { PipeTrapReveal.ApplyLook(PipeTrapLookYaw, PipeTrapLookPitch); }
    PipeTrapReveal.UnlockPlayer();
    bPipeTrapUseSequenceLook = false;
    bPipeTrapSequenceFinished = true;
    PipeTrapPhase = EMazePipeTrapPhase::Idle;
    if (PipeTrap.IsActive())
    {
        PipeTrapPhase = PipeTrap.AreWallsSwitched() ? EMazePipeTrapPhase::Closing : EMazePipeTrapPhase::Flipping;
    }
    SetComponentTickEnabled(PipeTrap.IsActive());
    OnPipeTrapRevealFinished.Broadcast();
    return true;
}

void UMazePuzzleComponent::AbortPipeTrapSequence()
{
    ClearPipeTrap();
    SetComponentTickEnabled(false);
}

bool UMazePuzzleComponent::LockPipeTrapPlayer(APawn* Player)
{
    const bool bAlreadyLocked = PipeTrapReveal.IsPlayerLocked();
    FString Error;
    if (!PipeTrapReveal.LockPlayer(Player, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Pipe reveal: %s"), *Error);
        return false;
    }
    if (!bAlreadyLocked)
    {
        bPipeTrapUseSequenceLook = false;
        PipeTrapLookYaw = 0.0f; PipeTrapLookPitch = 0.0f;
        bPipeTrapSequenceFinished = false;
    }
    return true;
}

void UMazePuzzleComponent::ReleasePipeTrapPlayer()
{
    PipeTrap.StopMovementAudio();
    const bool bWasLocked = PipeTrapReveal.IsPlayerLocked();
    PipeTrapReveal.Clear();
    bPipeTrapUseSequenceLook = false;
    PipeTrapLookYaw = 0.0f; PipeTrapLookPitch = 0.0f;
    if (bWasLocked && bPipeTrapTriggered) { OnPipeTrapRevealFinished.Broadcast(); }
}

void UMazePuzzleComponent::AdvancePipeTrap(float DeltaTime)
{
    if (!bPipeTrapTriggered || !MazeGenerator.IsValid()
        || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        ReleasePipeTrapPlayer();
        SetComponentTickEnabled(false);
        return;
    }
    if (PipeTrapReveal.IsPlayerLocked())
    {
        if (bPipeTrapUseSequenceLook) { PipeTrapReveal.ApplyLook(PipeTrapLookYaw, PipeTrapLookPitch); }
        if (PipeTrap.IsActive())
        {
            // The switch cue starts this trap. Allow its panel rotation during the input lock.
            PipeTrap.Advance(DeltaTime, false);
        }
        PipeTrapPhase = EMazePipeTrapPhase::Revealing;
        SetComponentTickEnabled(true);
        return;
    }
    if (!bPipeTrapSequenceFinished || !PipeTrap.IsActive())
    {
        SetComponentTickEnabled(false);
        return;
    }
    const bool bHitPlayer = PipeTrap.Advance(DeltaTime, true, PipeWallCloseSound.Get());
    PipeTrapPhase = PipeTrap.GetPhase();
    SetComponentTickEnabled(PipeTrap.IsActive());
    if (bHitPlayer)
    {
        if (ABaseGameMode* GameMode = Cast<ABaseGameMode>(UGameplayStatics::GetGameMode(this)))
        {
            // The helper latches contact before the existing loss/retry path can destroy this room.
            GameMode->TriggerLose();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe trap: %s could not find BaseGameMode. Check the map's GameMode Override."), *GetNameSafe(GetOwner()));
        }
    }
}

void UMazePuzzleComponent::ClearPipeTrap()
{
    ReleasePipeTrapPlayer();
    PipeTrap.Clear();
    bPipeTrapSequenceFinished = false;
    PipeTrapPhase = EMazePipeTrapPhase::Idle;
}

void UMazePuzzleComponent::RecordPipeSteamFailure()
{
    if (!bUsePipePuzzle || bUsePressureButtons || !bPrepared || !bArmed || !bPuzzleStarted || bPuzzleCompleted
        || !bPipeSteamResultShown || bPipeCheckUsesPreview || bPipeCheckIsStandalone || bPipeFailureRecorded
        || bPipeTrapTriggered || PipeFailureTallyCount <= 0 || !GetWorld() || !GetWorld()->IsGameWorld()
        || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze || ArePipeWallsCompleted())
    {
        return;
    }
    bPipeFailureRecorded = true;
    PipeFailedSteamChecks = FMath::Clamp(PipeFailedSteamChecks, 0, PipeFailureTallyCount - 1) + 1;
    UpdatePipeFailureTallies();
    PlayPipeFailureFeedback();
    if (PipeFailedSteamChecks == PipeFailureTallyCount)
    {
        // Latch and lock before notifying Blueprint so repeated input cannot start a second sequence.
        bPipeTrapTriggered = true;
        bPuzzleDoorUnlocked = false;
        bPipeSteamChecking = false;
        PipeSteamHoldRemaining = 0.0f;
        IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, true);
        UpdateStatusLights();
        PipeTrapPhase = EMazePipeTrapPhase::Revealing;
        SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || bEnemySpawningPending);
        if (!OnPipeTrapTriggered.IsBound())
        {
            UE_LOG(LogTemp, Error, TEXT("Pipe trap: Bind On Pipe Trap Triggered to this room's trap sequence."));
        }
        if (PipeTrapReveal.IsPlayerLocked()) { OnPipeTrapTriggered.Broadcast(); }
    }
}

bool UMazePuzzleComponent::GetPipeLeakOutlet(bool bRightWall, const FMazePipeLeak& Leak, FVector& Location, FVector& Direction) const
{
    Location = FVector::ZeroVector;
    Direction = FVector::ZeroVector;
    const FMazePipeLayout& Layout = bRightWall ? CheckedRightPipeLayout : CheckedLeftPipeLayout;
    if (Leak.Direction < 0 || Leak.Direction > 3 || !Layout.Cells.IsValidIndex(Leak.CellIndex)
        || !FMath::IsFinite(PipeLeakSteamOutletOffset) || PipeLeakSteamOutletOffset < 0.0f
        || !(FMazePipePuzzle::GetPorts(Layout.Cells[Leak.CellIndex]) & (1 << Leak.Direction)))
    {
        return false;
    }
    FMazePipeGridFrame Frame;
    FString Error;
    if (!BuildPipeFrame(bRightWall, Frame, Error) || Frame.Columns <= 0 || Frame.Rows <= 0
        || Frame.Columns != Layout.Columns || Frame.Rows != Layout.Rows
        || Layout.Cells.Num() != Frame.Columns * Frame.Rows
        || !FMath::IsFinite(Frame.CellSize) || Frame.CellSize <= 0.0f)
    {
        return false;
    }
    // Leak directions already include cell rotation. These are the fitted sleeve openings.
    const FVector Directions[] = { Frame.Up, Frame.Right, -Frame.Up, -Frame.Right };
    Direction = Directions[Leak.Direction].GetSafeNormal();
    Location = Frame.GetCellCenter(Leak.CellIndex) + Direction * (Frame.CellSize * 0.5f + PipeLeakSteamOutletOffset);
    if (FVector::DotProduct(Direction, FVector::UpVector) < -0.5)
    {
        const FVector TowardRoom = Frame.Inward.GetSafeNormal();
        if (!FMath::IsFinite(PipeDiameterFraction) || PipeDiameterFraction <= 0.0f
            || !FMath::IsFinite(PipeLeakSteamDownwardFrontClearance) || PipeLeakSteamDownwardFrontClearance < 0.0f
            || TowardRoom.ContainsNaN() || TowardRoom.Size() <= 0.0001)
        {
            return false;
        }
        // A rising plume must clear the cylinder radius before passing the pipe again.
        const double PipeRadius = Frame.CellSize * PipeDiameterFraction * 0.5;
        Location += TowardRoom * (PipeRadius + PipeLeakSteamDownwardFrontClearance);
    }
    return !Location.ContainsNaN() && !Direction.ContainsNaN() && Direction.Size() > 0.0001;
}

void UMazePuzzleComponent::SpawnPipeSteamLeaks(bool bRightWall, const FMazePipeSteamResult& Result)
{
    if (!IsValid(PipeLeakSteamSystem) || Result.bPassed)
    {
        return;
    }
    const float Scale = FMath::IsFinite(PipeLeakSteamScale) ? FMath::Clamp(PipeLeakSteamScale, 0.01f, 10.0f) : 1.0f;
    const float Density = FMath::IsFinite(PipeLeakSteamDensity) ? FMath::Clamp(PipeLeakSteamDensity, 0.0f, 100.0f) : 0.5f;
    const float SpeedMin = FMath::IsFinite(PipeLeakSteamSpeedMin) ? FMath::Clamp(PipeLeakSteamSpeedMin, 0.0f, 1000.0f) : 10.0f;
    const float SpeedMax = FMath::IsFinite(PipeLeakSteamSpeedMax) ? FMath::Clamp(PipeLeakSteamSpeedMax, SpeedMin, 1000.0f) : FMath::Max(SpeedMin, 30.0f);
    const float Strength = FMath::IsFinite(PipeLeakSteamDirectionStrength) ? FMath::Clamp(PipeLeakSteamDirectionStrength, 0.0f, 1000.0f) : 100.0f;
    for (const FMazePipeLeak& Leak : Result.Leaks)
    {
        FVector Location;
        FVector Direction;
        if (!Result.SteamDistances.IsValidIndex(Leak.CellIndex) || Result.SteamDistances[Leak.CellIndex] < 0
            || !GetPipeLeakOutlet(bRightWall, Leak, Location, Direction))
        {
            continue;
        }
        UNiagaraComponent* Steam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PipeLeakSteamSystem.Get(),
            Location, Direction.Rotation(), FVector(Scale), false, false, ENCPoolMethod::None, false);
        if (!IsValid(Steam))
        {
            continue;
        }
        Steam->SetVariableLinearColor(FName(TEXT("User.TrailColor")), PipeLeakSteamColor);
        Steam->SetVariableFloat(FName(TEXT("User.TrailDensity")), Density);
        Steam->SetVariableFloat(FName(TEXT("User.TrailSpeedMin")), SpeedMin);
        Steam->SetVariableFloat(FName(TEXT("User.TrailSpeedMax")), SpeedMax);
        Steam->SetVariableVec3(FName(TEXT("User.TrailVector")),
            bPipeLeakSteamVectorIsLocal ? FVector(Strength, 0, 0) : Direction * Strength);
        PipeLeakSteamComponents.Add(Steam);
        Steam->Activate(true);
    }
}

void UMazePuzzleComponent::PlayPipeFailureFeedback()
{
    if (!bUsePipePuzzle || !bPipeFailureRecorded || bPipeCheckUsesPreview || bPipeCheckIsStandalone
        || !GetWorld() || !GetWorld()->IsGameWorld())
    {
        return;
    }
    ClearPipeLeakSteam();
    if (IsValid(PipeWrongChoiceSound))
    {
        const float Volume = FMath::IsFinite(PipeWrongChoiceVolume) ? FMath::Clamp(PipeWrongChoiceVolume, 0.0f, 2.0f) : 1.0f;
        UGameplayStatics::PlaySound2D(this, PipeWrongChoiceSound.Get(), Volume);
    }
    if (bLeftPipeActive && !bLeftPipeCompleted)
    {
        SpawnPipeSteamLeaks(false, LeftPipeSteamResult);
    }
    if (bRightPipeActive && !bRightPipeCompleted)
    {
        SpawnPipeSteamLeaks(true, RightPipeSteamResult);
    }
    if (!PipeLeakSteamComponents.IsEmpty())
    {
        const float Duration = FMath::IsFinite(PipeLeakSteamDuration) ? FMath::Clamp(PipeLeakSteamDuration, 0.1f, 30.0f) : 4.0f;
        // A timer also stops the looping effect when the final failure disables component ticking.
        GetWorld()->GetTimerManager().SetTimer(PipeLeakSteamTimer, this, &UMazePuzzleComponent::ClearPipeLeakSteam, Duration, false);
    }
}

void UMazePuzzleComponent::ClearPipeLeakSteam()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PipeLeakSteamTimer);
    }
    for (UNiagaraComponent* Steam : PipeLeakSteamComponents)
    {
        if (IsValid(Steam))
        {
            Steam->DestroyComponent();
        }
    }
    PipeLeakSteamComponents.Reset();
}

bool UMazePuzzleComponent::ArePipeWallsCompleted() const
{
    return bUsePipePuzzle && (bLeftPipeActive || bRightPipeActive)
        && (!bLeftPipeActive || bLeftPipeCompleted) && (!bRightPipeActive || bRightPipeCompleted);
}

void UMazePuzzleComponent::AdvancePipeSteamCheck(float DeltaTime)
{
    if (!bPipeSteamChecking || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
    {
        return;
    }
    if (!bUsePipePuzzle)
    {
        ResetPipeLights();
        return;
    }
    if (bPipeSteamResultShown)
    {
        PipeSteamHoldRemaining -= DeltaTime;
        if (PipeSteamHoldRemaining <= 0.0f)
        {
            ResetPipeLights();
        }
        return;
    }
    PipeSteamElapsed += DeltaTime;
    const float StepSeconds = FMath::IsFinite(PipeSteamStepSeconds) ? FMath::Clamp(PipeSteamStepSeconds, 0.02f, 2.0f) : 0.2f;
    const float Progress = FMath::Min(static_cast<float>(LastPipeSteamStep), PipeSteamElapsed / StepSeconds);
    const int32 NextStep = FMath::Max(PipeSteamStep, FMath::FloorToInt(Progress));
    if (NextStep != PipeSteamStep)
    {
        PipeSteamStep = NextStep;
        ApplyPipeSteamStep();
    }
    if (PipeSteamStep == LastPipeSteamStep)
    {
        bPipeSteamResultShown = true;
        const bool bPassed = ArePipeWallsCompleted();
        PipeSteamHoldRemaining = !bPassed && FMath::IsFinite(PipeSteamHoldSeconds) ? FMath::Clamp(PipeSteamHoldSeconds, 0.0f, 30.0f) : 0.0f;
        if (bPassed)
        {
            ReleasePipeTrapPlayer();
            bPipeSteamChecking = false;
            SetComponentTickEnabled(PipeTrapReveal.IsPlayerLocked() || PipeTrap.IsActive() || bEnemySpawningPending
                || ShouldWatchRoomVisit() || (bArmed && bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked));
        }
        ApplyPipeSteamStep();
        if (bPipeCheckUsesPreview)
        {
            UE_LOG(LogTemp, Display, TEXT("Pipe light preview: %s. No gameplay failure or completion events were sent."), bPassed ? TEXT("Passed") : TEXT("Failed"));
        }
        else if (bPipeCheckIsStandalone)
        {
            UE_LOG(LogTemp, Display, TEXT("Pipe standalone test: %s. No maze completion or door events were sent."), bPassed ? TEXT("Passed") : TEXT("Failed"));
        }
        else
        {
            if (bPassed && !CompletePuzzle() && !bPuzzleCompleted)
            {
                UE_LOG(LogTemp, Error, TEXT("Pipe puzzle: Steam passed but completion was rejected for %s. Prepared=%d Armed=%d Started=%d Generator=%s LoopState=%d."),
                    *GetNameSafe(GetOwner()), bPrepared, bArmed, bPuzzleStarted, *GetNameSafe(MazeGenerator.Get()),
                    MazeGenerator.IsValid() ? static_cast<int32>(MazeGenerator->LoopState) : INDEX_NONE);
            }
            if (!bPassed)
            {
                RecordPipeSteamFailure();
            }
            OnPipeSteamCheckFinished.Broadcast(bPassed);
        }
    }
}