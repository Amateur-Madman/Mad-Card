// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazeSaveSubsystem.h"
#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeActors.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

const FString UMazeSaveSubsystem::TableSlot = TEXT("MadCard_Checkpoint");
const FString UMazeSaveSubsystem::LedgerSlot = TEXT("MadCard_RunState");

void UMazeSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ReadSavedData();
}

void UMazeSaveSubsystem::Deinitialize()
{
    bSaving = false;
    PendingSave = nullptr;
    DeathAfterSave.Reset();
    MazeGenerator.Reset();
    CurrentPlayer.Reset();
    PendingDeathLevel = NAME_None;
    Super::Deinitialize();
}

UMazeSaveSubsystem* UMazeSaveSubsystem::Get(const UObject* WorldContextObject)
{
    UGameInstance* Instance = UGameplayStatics::GetGameInstance(WorldContextObject);
    return IsValid(Instance) ? Instance->GetSubsystem<UMazeSaveSubsystem>() : nullptr;
}

bool UMazeSaveSubsystem::Fail(const FString& Message)
{
    UE_LOG(LogTemp, Error, TEXT("Maze save: %s"), *Message);
    OnSaveError.Broadcast(Message);
    return false;
}

void UMazeSaveSubsystem::ReadSavedData()
{
    TableSaves.SetNum(SaveSlotCount);
    OccupiedSlots.Init(false, SaveSlotCount);

    if (UGameplayStatics::DoesSaveGameExist(LedgerSlot, UserIndex))
    {
        RunLedger = Cast<UMazeRunLedger>(UGameplayStatics::LoadGameFromSlot(LedgerSlot, UserIndex));

        // Never fall back to an older charge count when the run record cannot be read.
        if (!IsValid(RunLedger) || RunLedger->SaveVersion != 1)
        {
            Fail(TEXT("The run-state file could not be read. Saved Hard runs are unavailable."));
            return;
        }
    }
    else
    {
        RunLedger = NewObject<UMazeRunLedger>(this);
    }

    bStorageReady = IsValid(RunLedger);

    for (int32 SlotIndex = 0; SlotIndex < SaveSlotCount; ++SlotIndex)
    {
        const FString SlotName = GetSlotName(SlotIndex);
        OccupiedSlots[SlotIndex] = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);

        if (!OccupiedSlots[SlotIndex])
        {
            continue;
        }

        TableSaves[SlotIndex] = Cast<UMazeSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));

        if (!IsValid(TableSaves[SlotIndex]) || TableSaves[SlotIndex]->SaveVersion != 1)
        {
            TableSaves[SlotIndex] = nullptr;
            Fail(FString::Printf(TEXT("Save Slot %d could not be read or uses an unsupported version."), SlotIndex + 1));
        }
    }
}

FString UMazeSaveSubsystem::GetSlotName(int32 SlotIndex) const
{
    return SlotIndex == 0 ? TableSlot : FString::Printf(TEXT("%s_%d"), *TableSlot, SlotIndex + 1);
}

int32 UMazeSaveSubsystem::GetSaveSlotCount() const
{
    return SaveSlotCount;
}

bool UMazeSaveSubsystem::CanLoadSlot(int32 SlotIndex) const
{
    const UMazeSaveGame* Save = TableSaves.IsValidIndex(SlotIndex) ? TableSaves[SlotIndex].Get() : nullptr;

    if (!bStorageReady || !IsValid(Save) || !Save->RunId.IsValid() || !Save->Checkpoint.bValid || Save->GameplayLevel.IsNone() || Save->MainMenuLevel.IsNone())
    {
        return false;
    }

    // Older progressed saves cannot identify which completed puzzle belonged to each door.
    for (const FName& PuzzleId : Save->Checkpoint.CompletedPuzzleIds)
    {
        bool bHasAssignment = false;
        for (const auto& Assignment : Save->Checkpoint.DoorPuzzleAssignments)
        {
            if (Assignment.Value == PuzzleId) { bHasAssignment = true; break; }
        }
        if (!bHasAssignment) { return false; }
    }

    const FMazeSavedRunState* State = RunLedger->RunStates.Find(Save->RunId);

    if (State && State->bRunEnded)
    {
        return false;
    }

    return Save->Difficulty == EMazeDifficulty::Easy || (Save->Difficulty == EMazeDifficulty::Hard && State != nullptr);
}

int32 UMazeSaveSubsystem::FindNewestSaveSlot() const
{
    int32 NewestSlot = INDEX_NONE;

    for (int32 SlotIndex = 0; SlotIndex < TableSaves.Num(); ++SlotIndex)
    {
        if (CanLoadSlot(SlotIndex) && (NewestSlot == INDEX_NONE || TableSaves[SlotIndex]->SavedAt > TableSaves[NewestSlot]->SavedAt))
        {
            NewestSlot = SlotIndex;
        }
    }

    return NewestSlot;
}

bool UMazeSaveSubsystem::HasSavedGame() const
{
    return FindNewestSaveSlot() != INDEX_NONE;
}

FMazeSaveSummary UMazeSaveSubsystem::GetSlotSummary(int32 SlotIndex) const
{
    FMazeSaveSummary Summary;

    if (!TableSaves.IsValidIndex(SlotIndex))
    {
        return Summary;
    }

    Summary.SlotIndex = SlotIndex;
    Summary.bSlotOccupied = OccupiedSlots[SlotIndex];
    Summary.bHasSave = CanLoadSlot(SlotIndex);
    const UMazeSaveGame* Save = TableSaves[SlotIndex].Get();

    if (IsValid(Save))
    {
        Summary.Difficulty = Save->Difficulty;
        Summary.MazesCompleted = Save->Checkpoint.RoomsCompleted;
        Summary.SavedAt = Save->SavedAt;
        const FMazeSavedRunState* State = IsValid(RunLedger) ? RunLedger->RunStates.Find(Save->RunId) : nullptr;
        Summary.SecondChanceCharges = Summary.Difficulty == EMazeDifficulty::Hard && State ? FMath::Clamp(State->SecondChanceCharges, 0, MaxSecondChanceCharges) : 0;
    }

    return Summary;
}

TArray<FMazeSaveSummary> UMazeSaveSubsystem::GetSaveSlots() const
{
    TArray<FMazeSaveSummary> Slots;

    for (int32 SlotIndex = 0; SlotIndex < SaveSlotCount; ++SlotIndex)
    {
        Slots.Add(GetSlotSummary(SlotIndex));
    }

    return Slots;
}

FMazeSaveSummary UMazeSaveSubsystem::GetSaveSummary() const
{
    return GetSlotSummary(FindNewestSaveSlot());
}

bool UMazeSaveSubsystem::IsBusy() const
{
    return bSaving || bTransitionPending || bRestorePending || bDeathPending;
}

bool UMazeSaveSubsystem::IsSaving() const
{
    return bSaving;
}

EMazeDifficulty UMazeSaveSubsystem::GetDifficulty() const
{
    return IsValid(ActiveRun) ? ActiveRun->Difficulty : EMazeDifficulty::Hard;
}

int32 UMazeSaveSubsystem::GetSecondChanceCharges() const
{
    return IsValid(ActiveRun) && GetDifficulty() == EMazeDifficulty::Hard ? FMath::Clamp(ActiveRun->SecondChanceCharges, 0, MaxSecondChanceCharges) : 0;
}

void UMazeSaveSubsystem::NotifyRunState()
{
    OnDifficultyChanged.Broadcast(GetDifficulty());
    OnSecondChanceChargesChanged.Broadcast(GetSecondChanceCharges());
}

bool UMazeSaveSubsystem::StartNewRun(EMazeDifficulty Difficulty, FName GameplayLevel, FName MainMenuLevel)
{
    if (IsBusy() || !bStorageReady || !GetWorld() || GameplayLevel.IsNone() || MainMenuLevel.IsNone() || (Difficulty != EMazeDifficulty::Easy && Difficulty != EMazeDifficulty::Hard))
    {
        return Fail(TEXT("New game needs Easy or Hard, valid gameplay/menu map names, and an idle save system."));
    }

    ActiveRun = NewObject<UMazeSaveGame>(this);
    ActiveRun->RunId = FGuid::NewGuid();
    ActiveRun->Difficulty = Difficulty;
    ActiveRun->GameplayLevel = GameplayLevel;
    ActiveRun->MainMenuLevel = MainMenuLevel;
    ActiveSaveSlot = INDEX_NONE;
    bRestoreFailed = false;
    bTransitionPending = true;
    MazeGenerator.Reset();
    CurrentPlayer.Reset();
    NotifyRunState();

    // A new game does not overwrite the old table save until a table is used.
    UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
    UGameplayStatics::OpenLevel(GetWorld(), GameplayLevel);
    return true;
}

bool UMazeSaveSubsystem::LoadSavedGame()
{
    return LoadSaveSlot(FindNewestSaveSlot());
}

bool UMazeSaveSubsystem::LoadSaveSlot(int32 SlotIndex)
{
    if (IsBusy() || !CanLoadSlot(SlotIndex) || !GetWorld())
    {
        OnLoadFinished.Broadcast(false);
        return Fail(TEXT("There is no available table save to load, or a transition is already running."));
    }

    ActiveRun = DuplicateObject<UMazeSaveGame>(TableSaves[SlotIndex].Get(), this);
    ActiveSaveSlot = SlotIndex;
    const FMazeSavedRunState* State = RunLedger->RunStates.Find(ActiveRun->RunId);
    ActiveRun->SecondChanceCharges = ActiveRun->Difficulty == EMazeDifficulty::Hard && State ? FMath::Clamp(State->SecondChanceCharges, 0, MaxSecondChanceCharges) : 0;
    bRestorePending = true;
    bRestoreFailed = false;
    bTransitionPending = true;
    MazeGenerator.Reset();
    CurrentPlayer.Reset();
    NotifyRunState();

    UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
    UGameplayStatics::OpenLevel(GetWorld(), ActiveRun->GameplayLevel);
    return true;
}

bool UMazeSaveSubsystem::WriteRunState(int32 Charges, bool bEnded)
{
    if (!bStorageReady || !IsValid(ActiveRun) || !ActiveRun->RunId.IsValid())
    {
        return Fail(TEXT("There is no active run to update."));
    }

    // Keep the old in-memory record if writing fails.
    UMazeRunLedger* Updated = DuplicateObject<UMazeRunLedger>(RunLedger, this);
    FMazeSavedRunState& State = Updated->RunStates.FindOrAdd(ActiveRun->RunId);
    State.SecondChanceCharges = FMath::Clamp(Charges, 0, MaxSecondChanceCharges);
    State.bRunEnded = bEnded;

    // This file is tiny. Finish this write before permitting a respawn or map transition.
    if (!UGameplayStatics::SaveGameToSlot(Updated, LedgerSlot, UserIndex))
    {
        return Fail(TEXT("Could not save the Second Chance/run state."));
    }

    RunLedger = Updated;
    return true;
}

bool UMazeSaveSubsystem::CanAddSecondChance() const
{
    return bStorageReady && IsValid(ActiveRun) && ActiveRun->Checkpoint.bValid && !IsBusy() && !bRestoreFailed && GetDifficulty() == EMazeDifficulty::Hard && GetSecondChanceCharges() < MaxSecondChanceCharges;
}

bool UMazeSaveSubsystem::AddSecondChanceCharge()
{
    if (!CanAddSecondChance())
    {
        return false;
    }

    const int32 NewCount = GetSecondChanceCharges() + 1;

    if (!WriteRunState(NewCount, false))
    {
        return false;
    }

    ActiveRun->SecondChanceCharges = NewCount;
    OnSecondChanceChargesChanged.Broadcast(NewCount);
    return true;
}

void UMazeSaveSubsystem::RegisterGenerator(AMazeGenerator* Generator)
{
    if (!IsValid(Generator) || Generator->GetWorld() != GetWorld())
    {
        return;
    }

    MazeGenerator = Generator;

    // Playing the gameplay map directly in the editor starts a fresh Hard run.
    if (!IsValid(ActiveRun))
    {
        ActiveRun = NewObject<UMazeSaveGame>(this);
        ActiveRun->RunId = FGuid::NewGuid();
        ActiveRun->GameplayLevel = FName(*UGameplayStatics::GetCurrentLevelName(Generator, true));
    }

    // A fresh run gets a new ID. Loading or retrying keeps the saved run's puzzle choices.
    Generator->SetPuzzleRunSeed(static_cast<int32>(FCrc::StrCrc32(*ActiveRun->RunId.ToString())));

    if (!bRestorePending)
    {
        UpdateCheckpoint();
    }
}

bool UMazeSaveSubsystem::RegisterPlayer(APawn* Player, FName EditorMainMenuLevel)
{
    if (!IsValid(Player) || !Player->IsPlayerControlled() || !MazeGenerator.IsValid() || MazeGenerator->GetWorld() != Player->GetWorld() || !IsValid(ActiveRun) || bRestoreFailed || bDeathPending)
    {
        return false;
    }

    CurrentPlayer = Player;

    if (ActiveRun->MainMenuLevel.IsNone())
    {
        ActiveRun->MainMenuLevel = EditorMainMenuLevel;
    }

    if (bRestorePending)
    {
        return TryRestoreCheckpoint();
    }

    if (!ActiveRun->Checkpoint.bValid)
    {
        return false;
    }

    const bool bNewRunStarting = bTransitionPending;
    bTransitionPending = false;
    if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
    {
        Controller->ResetIgnoreMoveInput();
        Controller->ResetIgnoreLookInput();
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->bShowMouseCursor = false;
        Player->EnableInput(Controller);
    }

    NotifyRunState();

    if (bNewRunStarting)
    {
        OnLoadFinished.Broadcast(true);
    }

    return true;
}

bool UMazeSaveSubsystem::UpdateCheckpoint()
{
    if (!IsValid(ActiveRun) || !MazeGenerator.IsValid() || bRestorePending || bRestoreFailed || bDeathPending)
    {
        return false;
    }

    FMazeStartRoomSaveData Checkpoint;

    if (!MazeGenerator->CaptureStartRoomState(Checkpoint))
    {
        return false;
    }

    // Arrival changes the death checkpoint, but only a table writes this progress to disk.
    ActiveRun->Checkpoint = Checkpoint;
    return true;
}

bool UMazeSaveSubsystem::CanSaveAtTable(APawn* Player) const
{
    return bStorageReady && IsValid(ActiveRun) && ActiveRun->Checkpoint.bValid && !ActiveRun->MainMenuLevel.IsNone() && !IsBusy() && !bRestoreFailed && IsValid(Player) && Player == CurrentPlayer.Get() && MazeGenerator.IsValid() && MazeGenerator->CanSaveAtCurrentStartRoom(Player);
}

bool UMazeSaveSubsystem::IsCurrentStartRoom(const AMazeActors* Room) const
{
    return IsValid(Room) && MazeGenerator.IsValid() && MazeGenerator->CurrentRoom == Room;
}

bool UMazeSaveSubsystem::SaveAtTable(APawn* Player)
{
    if (bSaving)
    {
        return false;
    }

    if (TableSaves.IsValidIndex(ActiveSaveSlot))
    {
        return SaveToSlot(Player, ActiveSaveSlot, true);
    }

    for (int32 SlotIndex = 0; SlotIndex < OccupiedSlots.Num(); ++SlotIndex)
    {
        if (!OccupiedSlots[SlotIndex])
        {
            return SaveToSlot(Player, SlotIndex);
        }
    }

    OnSaveFinished.Broadcast(false);
    return Fail(TEXT("All save slots are occupied. Open the save list and choose which slot to overwrite."));
}

bool UMazeSaveSubsystem::SaveToSlot(APawn* Player, int32 SlotIndex, bool bAllowOverwrite)
{
    // A second click must not finish or replace a write that is already running.
    if (bSaving)
    {
        return false;
    }

    if (!CanSaveAtTable(Player))
    {
        OnSaveFinished.Broadcast(false);
        return Fail(TEXT("Saving requires the current start room, a PlayerRespawn arrow, a configured menu map, and an idle save system."));
    }

    if (!TableSaves.IsValidIndex(SlotIndex) || (OccupiedSlots[SlotIndex] && !bAllowOverwrite))
    {
        OnSaveFinished.Broadcast(false);
        return Fail(TEXT("Choose a valid save slot and confirm before overwriting an occupied slot."));
    }

    PendingSave = DuplicateObject<UMazeSaveGame>(ActiveRun.Get(), this);
    PendingSave->SavedAt = FDateTime::UtcNow();
    PendingSaveSlot = SlotIndex;
    bSaving = true;
    OnSaveStarted.Broadcast(SlotIndex);

    if (!WriteRunState(GetSecondChanceCharges(), false))
    {
        HandleSaveCompleted(GetSlotName(SlotIndex), UserIndex, false);
        return false;
    }

    FAsyncSaveGameToSlotDelegate SavedDelegate;
    SavedDelegate.BindUObject(this, &UMazeSaveSubsystem::HandleSaveCompleted);

    // The menu stays open and displays Saving... until the completion delegate runs.
    UGameplayStatics::AsyncSaveGameToSlot(PendingSave.Get(), GetSlotName(SlotIndex), UserIndex, SavedDelegate);
    return true;
}

void UMazeSaveSubsystem::HandleSaveCompleted(const FString& SlotName, int32 SavedUserIndex, bool bSuccess)
{
    if (!bSaving || PendingSaveSlot == INDEX_NONE || SlotName != GetSlotName(PendingSaveSlot) || SavedUserIndex != UserIndex)
    {
        return;
    }

    if (bSuccess)
    {
        TableSaves[PendingSaveSlot] = PendingSave;
        OccupiedSlots[PendingSaveSlot] = true;
        ActiveSaveSlot = PendingSaveSlot;
    }

    PendingSave = nullptr;
    PendingSaveSlot = INDEX_NONE;
    bSaving = false;
    APawn* QueuedPlayer = DeathAfterSave.Get();
    DeathAfterSave.Reset();

    if (!bSuccess)
    {
        Fail(TEXT("Could not write the selected save slot."));
    }

    // If a death happened during the write, finish saving before consuming a charge.
    if (IsValid(QueuedPlayer))
    {
        HandlePlayerDeath(QueuedPlayer);
    }

    OnSaveListChanged.Broadcast();
    OnSaveFinished.Broadcast(bSuccess);
}

bool UMazeSaveSubsystem::TryRestoreCheckpoint()
{
    APawn* Player = CurrentPlayer.Get();
    APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;

    if (!bRestorePending || !MazeGenerator.IsValid() || !IsValid(Controller) || !IsValid(ActiveRun))
    {
        return false;
    }

    if (!MazeGenerator->RestoreStartRoomState(ActiveRun->Checkpoint))
    {
        bRestoreFailed = true;
        bRestorePending = false;
        bTransitionPending = false;
        OnLoadFinished.Broadcast(false);
        return Fail(TEXT("Could not restore the saved start room. Check its class, room tags, and PlayerRespawn arrow."));
    }

    const FTransform SpawnTransform = ActiveRun->Checkpoint.PlayerTransform;

    if (!Player->TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator()))
    {
        bRestoreFailed = true;
        bRestorePending = false;
        bTransitionPending = false;
        OnLoadFinished.Broadcast(false);
        return Fail(TEXT("The saved PlayerRespawn position is blocked. Move that arrow above the floor with room for the player capsule."));
    }

    if (UPawnMovementComponent* Movement = Player->GetMovementComponent())
    {
        Movement->StopMovementImmediately();
    }

    Controller->SetControlRotation(SpawnTransform.Rotator());
    Controller->ResetIgnoreMoveInput();
    Controller->ResetIgnoreLookInput();
    Controller->SetInputMode(FInputModeGameOnly());
    Controller->bShowMouseCursor = false;
    Player->EnableInput(Controller);
    bRestorePending = false;
    bTransitionPending = false;
    bDeathPending = false;
    NotifyRunState();
    OnLoadFinished.Broadcast(true);
    return true;
}

bool UMazeSaveSubsystem::HandlePlayerDeath(APawn* Player)
{
    // GameMode and OnPlayerLost can report the same death. Keep its first result.
    if (bDeathPending && IsValid(Player) && Player == CurrentPlayer.Get())
    {
        return true;
    }

    if (bSaving && IsValid(Player) && Player == CurrentPlayer.Get())
    {
        DeathAfterSave = Player;
        return true;
    }

    if (!IsValid(ActiveRun) || !IsValid(Player) || Player != CurrentPlayer.Get() || !GetWorld() || IsBusy() || bRestoreFailed)
    {
        return false;
    }

    bDeathPending = true;
    bDeathReturnsToMenu = GetDifficulty() == EMazeDifficulty::Hard && GetSecondChanceCharges() == 0;

    // Commit the result before any other listener can leave this level.
    if (bDeathReturnsToMenu)
    {
        if (!EndHardRun())
        {
            bRestoreFailed = true;
            bDeathPending = false;
            return false;
        }

        PendingDeathLevel = ActiveRun->MainMenuLevel;
    }
    else
    {
        if (!ActiveRun->Checkpoint.bValid || ActiveRun->GameplayLevel.IsNone())
        {
            bDeathPending = false;
            return Fail(TEXT("Respawning needs a valid start-room checkpoint and gameplay map."));
        }

        if (GetDifficulty() == EMazeDifficulty::Hard)
        {
            const int32 NewCount = GetSecondChanceCharges() - 1;

            if (!WriteRunState(NewCount, false))
            {
                bDeathPending = false;
                return false;
            }

            ActiveRun->SecondChanceCharges = NewCount;
            OnSecondChanceChargesChanged.Broadcast(NewCount);
        }

        PendingDeathLevel = ActiveRun->GameplayLevel;
    }

    if (PendingDeathLevel.IsNone())
    {
        bRestoreFailed = true;
        bDeathPending = false;
        return Fail(TEXT("Configure the gameplay and main menu map names before handling death."));
    }

    // A final Hard death goes to the menu. Other deaths wait for Play Again.
    if (bDeathReturnsToMenu)
    {
        bTransitionPending = true;
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UMazeSaveSubsystem::ProcessPendingDeath);
    }

    OnRespawnStateChanged.Broadcast(CanRespawnPlayer());
    return true;
}

bool UMazeSaveSubsystem::CanRespawnPlayer() const
{
    const bool bHasRun = IsValid(ActiveRun);
    const bool bHasCheckpoint = bHasRun && ActiveRun->Checkpoint.bValid;
    const bool bHasMap = bHasRun && !ActiveRun->GameplayLevel.IsNone();

    // Temporary: show which condition is keeping Play Again disabled.
    const bool bCanRespawn = bHasCheckpoint && bHasMap && GetWorld() && bDeathPending && !bDeathReturnsToMenu && !PendingDeathLevel.IsNone() && !bTransitionPending && !bRestorePending && !bRestoreFailed && !bSaving;

    UE_LOG(LogTemp, Warning, TEXT("Maze respawn: Allowed=%d Run=%d Checkpoint=%d Map=%d Death=%d Menu=%d PendingMap=%d Transition=%d Restore=%d Failed=%d Saving=%d"),
        bCanRespawn, bHasRun, bHasCheckpoint, bHasMap, bDeathPending, bDeathReturnsToMenu, !PendingDeathLevel.IsNone(), bTransitionPending, bRestorePending, bRestoreFailed, bSaving);

    return bCanRespawn;
}

bool UMazeSaveSubsystem::RespawnPlayer()
{
    if (!CanRespawnPlayer())
    {
        return false;
    }

    // Block repeated clicks before scheduling the reload. Do not spend another charge here.
    bTransitionPending = true;
    OnRespawnStateChanged.Broadcast(false);
    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UMazeSaveSubsystem::ProcessPendingDeath);
    return true;
}

bool UMazeSaveSubsystem::ReturnToMainMenu()
{
    if (!IsValid(ActiveRun) || ActiveRun->MainMenuLevel.IsNone() || !GetWorld() || bSaving || bTransitionPending || bRestorePending)
    {
        return false;
    }

    // Leaving voluntarily keeps the table saves. Only a final Hard death ends the run.
    PendingDeathLevel = ActiveRun->MainMenuLevel;
    bDeathReturnsToMenu = true;
    bDeathPending = true;
    bTransitionPending = true;
    OnRespawnStateChanged.Broadcast(false);
    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UMazeSaveSubsystem::ProcessPendingDeath);
    return true;
}

bool UMazeSaveSubsystem::EndHardRun()
{
    const bool bRecorded = WriteRunState(0, true);
    bool bDeleted = true;

    // Every slot from this run ends together. Saves from other runs remain available.
    for (int32 SlotIndex = 0; SlotIndex < TableSaves.Num(); ++SlotIndex)
    {
        const UMazeSaveGame* Save = TableSaves[SlotIndex].Get();

        if (!IsValid(Save) || Save->RunId != ActiveRun->RunId)
        {
            continue;
        }

        const FString SlotName = GetSlotName(SlotIndex);
        const bool bSlotDeleted = !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex) || UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
        bDeleted = bDeleted && bSlotDeleted;
        OccupiedSlots[SlotIndex] = !bSlotDeleted;

        if (bRecorded || bSlotDeleted)
        {
            TableSaves[SlotIndex] = nullptr;
        }
    }

    OnSaveListChanged.Broadcast();

    if (!bRecorded && !bDeleted)
    {
        // Also block the run in this session if both storage operations fail.
        FMazeSavedRunState& State = RunLedger->RunStates.FindOrAdd(ActiveRun->RunId);
        State.SecondChanceCharges = 0;
        State.bRunEnded = true;
        return Fail(TEXT("Could not invalidate or delete the ended Hard run. Check save-folder write access."));
    }

    if (!bDeleted)
    {
        UE_LOG(LogTemp, Warning, TEXT("Maze save: The checkpoint file could not be removed, but its ended run record prevents loading it."));
    }

    return true;
}

void UMazeSaveSubsystem::ProcessPendingDeath()
{
    if (!bDeathPending || !IsValid(ActiveRun) || !GetWorld())
    {
        return;
    }

    const FName LevelToOpen = PendingDeathLevel;
    PendingDeathLevel = NAME_None;
    UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());

    if (bDeathReturnsToMenu)
    {
        ActiveRun = nullptr;
        CurrentPlayer.Reset();
        MazeGenerator.Reset();
        bDeathPending = false;
        bDeathReturnsToMenu = false;
        bTransitionPending = false;
        bRestorePending = false;
        bRestoreFailed = false;
        ActiveSaveSlot = INDEX_NONE;
        NotifyRunState();
        UGameplayStatics::OpenLevel(GetWorld(), LevelToOpen);
        return;
    }

    // Reloading creates a living player through its normal BeginPlay. No player files change.
    bRestorePending = true;
    bRestoreFailed = false;
    bTransitionPending = true;
    bDeathPending = false;
    MazeGenerator.Reset();
    CurrentPlayer.Reset();
    UGameplayStatics::OpenLevel(GetWorld(), LevelToOpen);
}

void UMazeSaveSubsystem::ReportRestoreTimeout()
{
    bTransitionPending = false;
    bRestorePending = false;
    bRestoreFailed = true;
    OnLoadFinished.Broadcast(false);
    Fail(TEXT("Player/save setup timed out. Add MazePlayerSaveComponent to the possessed player, check MazeGenerator, and add PlayerRespawn to both start rooms."));
}   