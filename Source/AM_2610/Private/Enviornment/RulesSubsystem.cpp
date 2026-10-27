// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/RulesSubsystem.h"
#include "Enviornment/RulesMenu.h"
#include "Enviornment/MazePlayerSaveComponent.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

URulesSubsystem* URulesSubsystem::Get(const UObject* WorldContextObject)
{
    UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
    return IsValid(World) ? World->GetSubsystem<URulesSubsystem>() : nullptr;
}

bool URulesSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool URulesSubsystem::IsRulesOpen() const
{
    return IsValid(ActiveMenu);
}

bool URulesSubsystem::CanOpenRules(APawn* Player) const
{
    UWorld* World = GetWorld();
    APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);

    // Leave another menu, checkpoint restore, or death transition in control of input.
    return !bDeinitializing && !IsRulesOpen() && IsValid(World) && !World->bIsTearingDown && World->GetNetMode() == NM_Standalone
        && IsValid(Player) && Player->GetWorld() == World && Player->HasActorBegunPlay() && Player->IsLocallyControlled()
        && IsValid(Controller) && Controller->IsLocalController() && Controller->GetPawn() == Player
        && !UGameplayStatics::IsGamePaused(World) && !Controller->bShowMouseCursor && !Controller->IsMoveInputIgnored() && !Controller->IsLookInputIgnored()
        && (!IsValid(SaveSystem) || !SaveSystem->IsBusy());
}

void URulesSubsystem::RequestStartupRules(TSubclassOf<URulesMenu> MenuClass)
{
    UWorld* World = GetWorld();

    if (bDeinitializing || bShownStartupRules || bStartupPending || !IsValid(World) || World->GetNetMode() != NM_Standalone)
    {
        return;
    }

    StartupMenuClass = MenuClass;
    StartupAttempts = 0;
    ReadyChecks = 0;
    bStartupPending = true;

    // BeginPlay, possession, and save registration are allowed to finish first.
    World->GetTimerManager().SetTimer(StartupTimer, this, &URulesSubsystem::TryShowStartupRules, 0.1f, true, 0.25f);
}

void URulesSubsystem::TryShowStartupRules()
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || bDeinitializing)
    {
        return;
    }

    if (!bStartupPending || bShownStartupRules)
    {
        World->GetTimerManager().ClearTimer(StartupTimer);
        return;
    }

    APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
    UMazeSaveSubsystem* SaveSystem = UMazeSaveSubsystem::Get(this);
    bool bReady = CanOpenRules(Player);

    // RegisterPlayer changes input mode. Do not let it overwrite an already open popup.
    if (bReady && Player->FindComponentByClass<UMazePlayerSaveComponent>())
    {
        bReady = IsValid(SaveSystem) && SaveSystem->CanSaveAtTable(Player);
    }

    ReadyChecks = bReady ? ReadyChecks + 1 : 0;

    if (ReadyChecks >= 2)
    {
        if (!IsValid(OpenRules(Player, StartupMenuClass)))
        {
            bStartupPending = false;
            StartupMenuClass = nullptr;
            World->GetTimerManager().ClearTimer(StartupTimer);
        }

        return;
    }

    if (++StartupAttempts >= 300)
    {
        bStartupPending = false;
        StartupMenuClass = nullptr;
        World->GetTimerManager().ClearTimer(StartupTimer);
        UE_LOG(LogTemp, Warning, TEXT("Rules: Startup popup timed out. Check the possessed player, save checkpoint, other open menus, and RulesMenuClass. The terminal can still reopen the rules."));
    }
}

URulesMenu* URulesSubsystem::OpenRules(APawn* Player, TSubclassOf<URulesMenu> MenuClass)
{
    APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;

    // Calling this twice returns the existing popup without adding more input locks.
    if (IsRulesOpen())
    {
        return IsValid(Controller) && Controller == MenuController.Get() ? ActiveMenu.Get() : nullptr;
    }

    if (!CanOpenRules(Player))
    {
        return nullptr;
    }

    TSubclassOf<URulesMenu> Class = MenuClass;

    if (!Class)
    {
        Class = URulesMenu::StaticClass();
    }

    URulesMenu* Menu = CreateWidget<URulesMenu>(Controller, Class);

    if (!IsValid(Menu))
    {
        UE_LOG(LogTemp, Error, TEXT("Rules: Could not create RulesMenuClass."));
        return nullptr;
    }

    ActiveMenu = Menu;
    MenuController = Controller;
    bPreviousMouseCursor = Controller->bShowMouseCursor;
    Menu->SetRulesSystem(this);
    Menu->PrepareCloseInput(Controller->IsInputKeyDown(EKeys::E));
    Menu->SetIsFocusable(true);
    Menu->AddToViewport(100);

    if (ActiveMenu.Get() != Menu || !Menu->IsInViewport() || !Menu->HasRulesLayout())
    {
        UE_LOG(LogTemp, Error, TEXT("Rules: Use the empty-Designer built-in layout, or supply RulesTitle and RulesText."));
        CloseRules();
        return nullptr;
    }

    if (!UGameplayStatics::SetGamePaused(GetWorld(), true))
    {
        UE_LOG(LogTemp, Error, TEXT("Rules: Could not pause gameplay. Check the game mode's pause settings."));
        CloseRules();
        return nullptr;
    }

    bPausedByRules = true;
    Controller->SetIgnoreMoveInput(true);
    Controller->SetIgnoreLookInput(true);
    bLockedPlayerInput = true;

    if (UPawnMovementComponent* Movement = Player->GetMovementComponent())
    {
        Movement->StopMovementImmediately();
    }

    Controller->FlushPressedKeys();
    Controller->bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Menu->TakeWidget());
    Controller->SetInputMode(InputMode);
    Menu->SetKeyboardFocus();

    bShownStartupRules = true;
    bStartupPending = false;
    StartupMenuClass = nullptr;
    GetWorld()->GetTimerManager().ClearTimer(StartupTimer);
    return Menu;
}

void URulesSubsystem::ReleasePlayerInput()
{
    APlayerController* Controller = MenuController.Get();
    UWorld* World = GetWorld();
    const bool bRestoreInput = !bDeinitializing && IsValid(World) && !World->bIsTearingDown;

    if (IsValid(Controller) && bLockedPlayerInput)
    {
        // Balance this popup's locks without resetting locks owned by another system.
        Controller->SetIgnoreMoveInput(false);
        Controller->SetIgnoreLookInput(false);
    }

    if (bPausedByRules && bRestoreInput)
    {
        UGameplayStatics::SetGamePaused(World, false);
    }

    if (IsValid(Controller) && bRestoreInput && Controller->GetWorld() == World)
    {
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->bShowMouseCursor = bPreviousMouseCursor;
        Controller->FlushPressedKeys();
    }

    bLockedPlayerInput = false;
    bPausedByRules = false;
    MenuController.Reset();
}

void URulesSubsystem::CloseRules()
{
    URulesMenu* Menu = ActiveMenu.Get();
    ActiveMenu = nullptr;

    if (IsValid(Menu))
    {
        Menu->SetRulesSystem(nullptr);
        Menu->RemoveFromParent();
    }

    ReleasePlayerInput();
}

void URulesSubsystem::NotifyMenuRemoved(URulesMenu* Menu)
{
    if (ActiveMenu.Get() == Menu)
    {
        ActiveMenu = nullptr;
        ReleasePlayerInput();
    }
}

void URulesSubsystem::Deinitialize()
{
    bDeinitializing = true;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StartupTimer);
    }

    bStartupPending = false;
    StartupMenuClass = nullptr;
    CloseRules();
    Super::Deinitialize();
}