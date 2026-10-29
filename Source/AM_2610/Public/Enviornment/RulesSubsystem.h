// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "RulesSubsystem.generated.h"

class APawn;
class APlayerController;
class URulesMenu;

// World lifetime means a new level load can show the rules again without changing saves.
UCLASS()
class AM_2610_API URulesSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Rules", meta = (WorldContext = "WorldContextObject"))
    static URulesSubsystem* Get(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "Rules")
    bool CanOpenRules(APawn* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Rules")
    URulesMenu* OpenRules(APawn* Player, TSubclassOf<URulesMenu> MenuClass = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Rules")
    void CloseRules();

    UFUNCTION(BlueprintPure, Category = "Rules")
    bool IsRulesOpen() const;

    UFUNCTION(BlueprintPure, Category = "Rules")
    bool HasShownStartupRules() const { return bShownStartupRules; }

    // Rebuilt or newly generated start rooms must not display another startup popup.
    void RequestStartupRules(TSubclassOf<URulesMenu> MenuClass);
    void NotifyMenuRemoved(URulesMenu* Menu);

    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    UPROPERTY(Transient)
    TObjectPtr<URulesMenu> ActiveMenu;

    UPROPERTY(Transient)
    TSubclassOf<URulesMenu> StartupMenuClass;

    TWeakObjectPtr<APlayerController> MenuController;
    FTimerHandle StartupTimer;
    int32 StartupAttempts = 0;
    int32 ReadyChecks = 0;
    bool bStartupPending = false;
    bool bShownStartupRules = false;
    bool bLockedPlayerInput = false;
    bool bPausedByRules = false;
    bool bPreviousMouseCursor = false;
    bool bDeinitializing = false;

    void TryShowStartupRules();
    void ReleasePlayerInput();
};