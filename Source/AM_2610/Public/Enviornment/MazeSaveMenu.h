// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/Button.h"
#include "MazeSaveMenu.generated.h"

class APawn;
class APlayerController;
class UButtonWithText;
class UWidget;
class UTextBlock;
class UVerticalBox;
class UMazeSaveMenu;
class UMazeSaveSubsystem;

UENUM(BlueprintType)
enum class EMazeSaveMenuMode : uint8
{
    Load,
    Save
};

// Each button keeps its own slot index instead of adding a separate event for every slot.
UCLASS()
class AM_2610_API UMazeSaveSlotButton : public UButton
{
    GENERATED_BODY()

public:
    void SetupSlot(UMazeSaveMenu* Menu, int32 SlotIndex);

private:
    TWeakObjectPtr<UMazeSaveMenu> SaveMenu;
    int32 SaveSlotIndex = INDEX_NONE;

    UFUNCTION()
    void HandleClicked();
};

// A styled button can be any Blueprint derived from the existing ButtonWithText class.
// This small object keeps its slot index without changing that shared button class.
UCLASS()
class AM_2610_API UMazeSaveSlotBinding : public UObject
{
    GENERATED_BODY()

public:
    void SetupSlot(UMazeSaveMenu* Menu, int32 SlotIndex, UButtonWithText* Button);
    void ClearBinding();

private:
    TWeakObjectPtr<UMazeSaveMenu> SaveMenu;
    TWeakObjectPtr<UButtonWithText> SlotButton;
    int32 SaveSlotIndex = INDEX_NONE;

    UFUNCTION()
    void HandleClicked();
};

// A separate overlay. The existing player and HUD classes do not need any changes.
UCLASS()
class AM_2610_API UMazeSaveMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    // Load opens over the main menu. Save opens over the game from a start-room table.
    UFUNCTION(BlueprintCallable, Category = "Maze|Save", meta = (WorldContext = "WorldContextObject"))
    static UMazeSaveMenu* OpenSaveMenu(const UObject* WorldContextObject, EMazeSaveMenuMode Mode, APawn* Player = nullptr, TSubclassOf<UMazeSaveMenu> MenuClass = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    void SelectSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    void ConfirmSelection();

    UFUNCTION(BlueprintCallable, Category = "Maze|Save")
    void CloseSaveMenu();

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    EMazeSaveMenuMode MenuMode = EMazeSaveMenuMode::Load;

    UPROPERTY(BlueprintReadOnly, Category = "Maze|Save")
    int32 SelectedSlotIndex = INDEX_NONE;

    // Select WBP_ButtonWithText to reuse the main menu's font, underline, and animations.
    // Leave empty to keep the original plain buttons.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maze|Save|Style")
    TSubclassOf<UButtonWithText> ButtonWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maze|Save|Style", meta = (ClampMin = "24.0"))
    float SlotButtonHeight = 104.0f;

    // Copy the Settings title font here. An empty font family keeps the existing title.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maze|Save|Style", meta = (DisplayName = "Title Font"))
    FSlateFontInfo MenuTitleFont;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Optional named widgets let a teammate replace the layout in a Widget Blueprint.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> SlotsBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> ConfirmButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ConfirmText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButton;

    // A custom layout can use these instead of ConfirmButton/ConfirmText and BackButton.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButtonWithText> StyledConfirmButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButtonWithText> StyledBackButton;

private:
    TWeakObjectPtr<UMazeSaveSubsystem> SaveSystem;
    TWeakObjectPtr<APawn> SavePlayer;
    TWeakObjectPtr<APlayerController> MenuController;
    bool bLockedPlayerInput = false;
    bool bPreviousMouseCursor = false;

    // Keep rows alive during refreshes so their focus and underline animation are preserved.
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWidget>> SlotWidgets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMazeSaveSlotBinding>> SlotBindings;

    void ClearSlotBindings();
    void BuildDefaultLayout();
    void RefreshControls();
    void ReleasePlayerInput();

    UFUNCTION()
    void RefreshSlots();

    UFUNCTION()
    void HandleSaveStarted(int32 SlotIndex);

    UFUNCTION()
    void HandleSaveFinished(bool bSuccess);

    UFUNCTION()
    void HandleSaveError(const FString& Message);
};