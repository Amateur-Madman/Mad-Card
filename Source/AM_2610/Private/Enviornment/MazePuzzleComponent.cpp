// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePuzzleComponent.h"
#include "Enviornment/MazeActors.h"
#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeInterface.h"
#include "Enviornment/MazePuzzleButton.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

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
    IMazeInterface::Execute_SetMazeDoorLocked(Room, PuzzleDoorTag, false);
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

    if (!bArmed || !bWaitingForEnemyEntry || !IsValid(OverlappedComponent) || OverlappedComponent != EnemyEntryTrigger.Get() || OverlappedComponent->GetOwner() != GetOwner() || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
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
    SetComponentTickEnabled(bArmed && (bEnemySpawningPending || (bPuzzleStarted && bUsePressureButtons && !bButtonsUnlocked)));
    EnableEnemyEntryTrigger();
}

bool UMazePuzzleComponent::BeginPuzzle(APawn* Player)
{
    if (!bArmed || !bUsePressureButtons || bPuzzleStarted || bPuzzleCompleted || !IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze)
    {
        return false;
    }

    // Lock once on entry. Extra overlaps and later re-entry do not reset it.
    bPuzzleStarted = true;
    bPuzzleDoorUnlocked = false;
    bConsoleRed = true;
    IMazeInterface::Execute_SetMazeDoorLocked(GetOwner(), PuzzleDoorTag, true);
    OnConsoleStateChanged.Broadcast(true);
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

    AdvanceEntryEnemySpawning(DeltaTime);

    if (!bArmed || !bPuzzleStarted || bButtonsUnlocked || !MazeGenerator.IsValid() || MazeGenerator->LoopState != EMazeLoopState::TraversingMaze || SpawnedButtons.Num() != 2 || !IsValid(SpawnedButtons[0]) || !IsValid(SpawnedButtons[1]))
    {
        if (!bEnemySpawningPending)
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
        SetComponentTickEnabled(bEnemySpawningPending);
        OnConsoleStateChanged.Broadcast(false);
        OnButtonsUnlocked.Broadcast();
    }
}

bool UMazePuzzleComponent::CompletePuzzle()
{
    if (!bArmed || bPuzzleCompleted || !bButtonsUnlocked || !bPuzzleDoorUnlocked || (bUsePressureButtons && !bPuzzleStarted) || !MazeGenerator.IsValid() || !MazeGenerator->RegisterPuzzleCompleted(Cast<AMazeActors>(GetOwner())))
    {
        return false;
    }

    bPuzzleCompleted = true;
    SetComponentTickEnabled(bEnemySpawningPending);
    OnPuzzleCompleted.Broadcast();
    return true;
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

    SpawnedButtons.Reset();
    DebugButtons.Reset();
    SpawnedWeepingAngels.Reset();
}

void UMazePuzzleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bArmed = false;
    SetComponentTickEnabled(false);
    ClearEnemyEntryTrigger();
    ClearSpawnedActors();
    Super::EndPlay(EndPlayReason);
}