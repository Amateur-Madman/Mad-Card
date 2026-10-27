// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enviornment/MazeSaveGame.h"
#include "MazeSaveSubsystem.generated.h"

class AMazeGenerator;
class AMazeActors;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeSecondChanceChanged, int32, Charges);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeDifficultyChanged, EMazeDifficulty, Difficulty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeSaveFinished, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeLoadFinished, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeSaveError, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeSaveStarted, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMazeSaveListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMazeRespawnStateChanged, bool, bCanRespawn);

// Shared by the menu, table, pickups, and player component. No widget code lives here.
UCLASS(BlueprintType)
class AM_2610_API UMazeSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Maze|Save", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Maze Save System"))
    static UMazeSaveSubsystem* Get(const UObject* WorldContextObject);

    // Call this from the Easy or Hard button. Pass the real gameplay and menu map names.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool StartNewRun(EMazeDifficulty Difficulty, FName GameplayLevel, FName MainMenuLevel);

    // Kept for existing callers. Loads the newest available slot.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool LoadSavedGame();

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool HasSavedGame() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    FMazeSaveSummary GetSaveSummary() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    int32 GetSaveSlotCount() const;

    // Includes empty slots so the same list can be used for saving and loading.
    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    TArray<FMazeSaveSummary> GetSaveSlots() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    FMazeSaveSummary GetSlotSummary(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool LoadSaveSlot(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool IsBusy() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool IsSaving() const;

    // Kept for existing callers. Uses the active slot, or the first empty slot.
    // Both saving functions report the disk result through OnSaveFinished.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool SaveAtTable(APawn* Player);

    // True means the request started. OnSaveFinished reports the actual disk result.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool SaveToSlot(APawn* Player, int32 SlotIndex, bool bAllowOverwrite = false);

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool CanSaveAtTable(APawn* Player) const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    EMazeDifficulty GetDifficulty() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    int32 GetSecondChanceCharges() const;

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool CanAddSecondChance() const;

    // A Second Chance pickup calls this instead of adding anything to CardHand.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool AddSecondChanceCharge();

    // Records the death once. Allowed respawns wait for the Play Again button.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool HandlePlayerDeath(APawn* Player);

    UFUNCTION(BlueprintPure, Category = "Maze|Save")
    bool CanRespawnPlayer() const;

    // Restores the current start room, rather than starting a new run or loading a table slot.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool RespawnPlayer();

    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    bool ReturnToMainMenu();

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeRespawnStateChanged OnRespawnStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeSecondChanceChanged OnSecondChanceChargesChanged;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeDifficultyChanged OnDifficultyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeSaveFinished OnSaveFinished;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeSaveStarted OnSaveStarted;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeSaveListChanged OnSaveListChanged;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeLoadFinished OnLoadFinished;

    UPROPERTY(BlueprintAssignable, Category = "Maze|Save")
    FOnMazeSaveError OnSaveError;

    // MazeGenerator and MazePlayerSaveComponent connect themselves through these functions.
    void RegisterGenerator(AMazeGenerator* Generator);
    bool RegisterPlayer(APawn* Player, FName EditorMainMenuLevel);
    bool UpdateCheckpoint();
    void ReportRestoreTimeout();
    bool IsCurrentStartRoom(const AMazeActors* Room) const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UMazeSaveGame> ActiveRun;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMazeSaveGame>> TableSaves;

    // Keep the snapshot alive until the asynchronous write finishes.
    UPROPERTY(Transient)
    TObjectPtr<UMazeSaveGame> PendingSave;

    UPROPERTY(Transient)
    TObjectPtr<UMazeRunLedger> RunLedger;

    TWeakObjectPtr<AMazeGenerator> MazeGenerator;
    TWeakObjectPtr<APawn> CurrentPlayer;
    TWeakObjectPtr<APawn> DeathAfterSave;
    TArray<bool> OccupiedSlots;
    FName PendingDeathLevel = NAME_None;
    int32 ActiveSaveSlot = INDEX_NONE;
    int32 PendingSaveSlot = INDEX_NONE;

    bool bStorageReady = false;
    bool bTransitionPending = false;
    bool bRestorePending = false;
    bool bRestoreFailed = false;
    bool bDeathPending = false;
    bool bDeathReturnsToMenu = false;
    bool bSaving = false;

    // Keep the original filename for Slot 1 so existing saves remain available.
    static const FString TableSlot;
    static const FString LedgerSlot;
    static constexpr int32 SaveSlotCount = 3;
    static constexpr int32 UserIndex = 0;
    static constexpr int32 MaxSecondChanceCharges = 1;

    void ReadSavedData();
    FString GetSlotName(int32 SlotIndex) const;
    bool CanLoadSlot(int32 SlotIndex) const;
    int32 FindNewestSaveSlot() const;
    void HandleSaveCompleted(const FString& SlotName, int32 SavedUserIndex, bool bSuccess);
    bool WriteRunState(int32 Charges, bool bEnded);
    bool TryRestoreCheckpoint();
    void ProcessPendingDeath();
    bool EndHardRun();
    void NotifyRunState();
    bool Fail(const FString& Message);
};
