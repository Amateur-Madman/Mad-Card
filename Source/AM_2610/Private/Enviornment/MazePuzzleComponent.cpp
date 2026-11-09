// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePuzzleComponent.h"
#include "Enviornment/MazeActors.h"
#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeInterface.h"
#include "Enviornment/MazePuzzleButton.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
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
            const FVector Up = Transform.InverseTransformVectorNoScale(Capsule->GetUpVector());
            Extent = FVector(Radius) + Up.GetAbs() * FMath::Max(0.0f, HalfHeight - Radius);
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
}

bool UMazePuzzleComponent::PreparePuzzle(AMazeGenerator* Generator, FName SelectedId, TSubclassOf<APawn> AngelClass, int32 Seed, bool bSpawnAllButtons, bool bSpawnAllEnemies)
{
    AMazeActors* Room = Cast<AMazeActors>(GetOwner());

    if (bPrepared || !IsValid(Generator) || !IsValid(Room) || SelectedId.IsNone() || !Room->GetClass()->ImplementsInterface(UMazeInterface::StaticClass()) || EnemySpawnTag.IsNone() || PuzzleDoorTag.IsNone() || ButtonSpawnTag.IsNone() || ButtonSpawnTag == EnemySpawnTag)
    {
        return false;
    }

    RequestedEnemyCount = FMath::Clamp(WeepingAngelCount, 0, 16);

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

    if (!bArmed || bPuzzleCompleted || CompletedExitArea.IsValid() || !bWaitingForEnemyEntry || !IsValid(OverlappedComponent) || OverlappedComponent != EnemyEntryTrigger.Get() || OverlappedComponent->GetOwner() != GetOwner() || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return;
    }

    // Record the entry before spawning so extra overlaps cannot start it twice.
    bWaitingForEnemyEntry = false;
    bEnemySpawningPending = true;
    EnemySpawnRetryRemaining = 0.0f;

    UBoxComponent* Trigger = EnemyEntryTrigger.Get();
    Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &UMazePuzzleComponent::OnEnemyEntryTriggerBeginOverlap);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Start the pressure plate puzzle when the player enters.
    if (bUsePressureButtons && !bPuzzleStarted)
    {
        BeginPuzzle(Player);
    }

    UE_LOG(LogTemp, Display, TEXT("Puzzle: Player entered %s. Starting the room's Weeping Angel spawns."), *GetNameSafe(GetOwner()));

    AdvanceEntryEnemySpawning(0.0f);
    SetComponentTickEnabled(bEnemySpawningPending || (bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked));
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
    SetComponentTickEnabled(bArmed && (CompletedExitArea.IsValid() || bEnemySpawningPending || (bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked)));
    if (!CompletedExitArea.IsValid())
    {
        EnableEnemyEntryTrigger();
    }
}

bool UMazePuzzleComponent::BeginPuzzle(APawn* Player)
{
    if (!bArmed || !bUsePressureButtons || bPuzzleStarted || bPuzzleCompleted || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
    }

    const UBoxComponent* Area = CompletedExitArea.Get();
    if (Area && (!Area->K2_IsQueryCollisionEnabled() || !IsPlayerFullyInsideRoomArea(*Area, *Player)))
    {
        return false;
    }

    const bool bTurnConsoleRed = !bButtonsUnlocked && !bConsoleRed;
    bPuzzleStarted = true;
    bPuzzleDoorUnlocked = false;
    bConsoleRed = !bButtonsUnlocked;
    IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, true);
    UpdateStatusLights();
    if (bTurnConsoleRed)
    {
        OnConsoleStateChanged.Broadcast(true);
    }
    SetComponentTickEnabled(true);
    return true;
}

bool UMazePuzzleComponent::ActivateConsole(APawn* Player)
{
    if (!bArmed || !bPuzzleStarted || !bButtonsUnlocked || bPuzzleDoorUnlocked || bPuzzleCompleted || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
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

    const UBoxComponent* Area = CompletedExitArea.Get();
    const bool bWatchRoom = bArmed && !bPuzzleCompleted && Area && MazeGenerator.IsValid()
        && MazeGenerator->LoopState == EMazeLoopState::TraversingMaze;
    if (bWatchRoom && !UpdateRoomVisit(*Area))
    {
        return;
    }

    AdvanceEntryEnemySpawning(DeltaTime);

    if (!bArmed || !bPuzzleStarted || bButtonsUnlocked || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze || SpawnedButtons.Num() != 2 || !IsValid(SpawnedButtons[0]) || !IsValid(SpawnedButtons[1]))
    {
        if (!bWatchRoom && !bEnemySpawningPending)
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
        SetComponentTickEnabled(bWatchRoom || bEnemySpawningPending);
        OnConsoleStateChanged.Broadcast(false);
        OnButtonsUnlocked.Broadcast();
    }
}

bool UMazePuzzleComponent::UpdateRoomVisit(const UBoxComponent& Area)
{
    if (!Area.K2_IsQueryCollisionEnabled() || !Area.GetGenerateOverlapEvents()
        || Area.GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
    {
        return false;
    }

    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bPlayerValid = IsValid(Player) && Player->IsPlayerControlled();
    const UCapsuleComponent* Capsule = bPlayerValid ? Cast<UCapsuleComponent>(Player->GetRootComponent()) : nullptr;
    const bool bPlayerOverlapping = bPlayerValid && (Capsule ? Area.IsOverlappingComponent(Capsule) : Area.IsOverlappingActor(Player));
    if (bPlayerOverlapping)
    {
        if (!IsPlayerFullyInsideRoomArea(Area, *Player))
        {
            return false;
        }

        if (!bRoomVisitActive)
        {
            bRoomVisitActive = true;
            if (bUsePressureButtons && !bPuzzleStarted)
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

    // Start the puzzle again on entry if the console still needs to be used.
    bPuzzleStarted = bPuzzleStarted && bPuzzleDoorUnlocked;
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

bool UMazePuzzleComponent::CompletePuzzle()
{
    if (!bArmed || bPuzzleCompleted || !bButtonsUnlocked || !bPuzzleDoorUnlocked || (bUsePressureButtons && !bPuzzleStarted) || !MazeGenerator.IsValid() || !MazeGenerator->RegisterPuzzleCompleted(Cast<AMazeActors>(GetOwner())))
    {
        return false;
    }

    bPuzzleCompleted = true;
    UpdateStatusLights();
    SetComponentTickEnabled(bEnemySpawningPending);
    QueueCompletedExitCheck();
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
        || Area->GetCollisionEnabled() == ECollisionEnabled::NoCollision || Area->GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly
        || !Area->GetGenerateOverlapEvents() || Area->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
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
    if (Area->GetCollisionEnabled() == ECollisionEnabled::NoCollision || Area->GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly
        || !Area->GetGenerateOverlapEvents() || Area->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
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
    bArmed = false;
    SetComponentTickEnabled(false);
    ClearEnemyEntryTrigger();
    ClearCompletedExitArea();
    StatusLightMaterials.Reset();
    StatusLightMeshes.Reset();
    ClearSpawnedActors();
    Super::EndPlay(EndPlayReason);
}