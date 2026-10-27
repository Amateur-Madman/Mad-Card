// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazeSaveMenu.h"
#include "Enviornment/MazeSaveSubsystem.h"
#include "Both/ButtonWithText.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMazeSaveSlotButton::SetupSlot(UMazeSaveMenu* Menu, int32 SlotIndex)
{
    SaveMenu = Menu;
    SaveSlotIndex = SlotIndex;
    OnClicked.AddUniqueDynamic(this, &UMazeSaveSlotButton::HandleClicked);
}

void UMazeSaveSlotButton::HandleClicked()
{
    if (SaveMenu.IsValid())
    {
        SaveMenu->SelectSlot(SaveSlotIndex);
    }
}

void UMazeSaveSlotBinding::SetupSlot(UMazeSaveMenu* Menu, int32 SlotIndex, UButtonWithText* Button)
{
    ClearBinding();
    SaveMenu = Menu;
    SaveSlotIndex = SlotIndex;
    SlotButton = Button;

    if (SlotButton.IsValid())
    {
        SlotButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMazeSaveSlotBinding::HandleClicked);
    }
}

void UMazeSaveSlotBinding::ClearBinding()
{
    if (SlotButton.IsValid())
    {
        SlotButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMazeSaveSlotBinding::HandleClicked);
    }

    SlotButton.Reset();
    SaveMenu.Reset();
    SaveSlotIndex = INDEX_NONE;
}

void UMazeSaveSlotBinding::HandleClicked()
{
    if (SaveMenu.IsValid())
    {
        SaveMenu->SelectSlot(SaveSlotIndex);
    }
}

UMazeSaveMenu* UMazeSaveMenu::OpenSaveMenu(const UObject* WorldContextObject, EMazeSaveMenuMode Mode, APawn* Player, TSubclassOf<UMazeSaveMenu> MenuClass)
{
    UMazeSaveSubsystem* System = UMazeSaveSubsystem::Get(WorldContextObject);
    APlayerController* Controller = UGameplayStatics::GetPlayerController(WorldContextObject, 0);

    if (!IsValid(System) || !IsValid(Controller) || System->IsBusy() || (Mode == EMazeSaveMenuMode::Save && (!System->CanSaveAtTable(Player) || Player->GetController() != Controller)))
    {
        return nullptr;
    }

    TArray<UUserWidget*> OpenMenus;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, OpenMenus, StaticClass(), true);

    // Repeated interaction must not stack menus or stack the player's input locks.
    for (UUserWidget* Widget : OpenMenus)
    {
        if (UMazeSaveMenu* Existing = Cast<UMazeSaveMenu>(Widget))
        {
            return Existing;
        }
    }

    TSubclassOf<UMazeSaveMenu> Class = MenuClass;

    if (!Class)
    {
        Class = StaticClass();
    }

    UMazeSaveMenu* Menu = CreateWidget<UMazeSaveMenu>(Controller, Class);

    if (!IsValid(Menu))
    {
        return nullptr;
    }

    Menu->MenuMode = Mode;
    Menu->SavePlayer = Player;
    Menu->MenuController = Controller;
    Menu->bPreviousMouseCursor = Controller->bShowMouseCursor;
    Menu->SetIsFocusable(true);
    Menu->AddToViewport(100);

    if (Mode == EMazeSaveMenuMode::Save)
    {
        Controller->SetIgnoreMoveInput(true);
        Controller->SetIgnoreLookInput(true);
        Menu->bLockedPlayerInput = true;

        if (UPawnMovementComponent* Movement = Player->GetMovementComponent())
        {
            Movement->StopMovementImmediately();
        }
    }

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Menu->TakeWidget());
    Controller->SetInputMode(InputMode);
    Controller->bShowMouseCursor = true;
    Menu->SetKeyboardFocus();
    return Menu;
}

TSharedRef<SWidget> UMazeSaveMenu::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    // The native class has a working layout even without a new Widget Blueprint.
    if (!WidgetTree->RootWidget)
    {
        BuildDefaultLayout();
    }

    return Super::RebuildWidget();
}

void UMazeSaveMenu::BuildDefaultLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SaveCanvas"));
    WidgetTree->RootWidget = Root;

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.96f));
    UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackgroundSlot->SetOffsets(FMargin(0.0f));

    TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
    FSlateFontInfo TitleFont = TitleText->GetFont();
    TitleFont.Size = 32;
    TitleText->SetFont(TitleFont);
    UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(TitleText);
    TitleSlot->SetAnchors(FAnchors(0.5f, 0.1f));
    TitleSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    TitleSlot->SetAutoSize(true);

    // This stays at the top of the screen while the asynchronous save is running.
    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 26;
    StatusText->SetFont(StatusFont);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f)));
    StatusText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* StatusSlot = Root->AddChildToCanvas(StatusText);
    StatusSlot->SetAnchors(FAnchors(0.5f, 0.04f));
    StatusSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    StatusSlot->SetAutoSize(true);

    SlotsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotsBox"));
    UCanvasPanelSlot* ListSlot = Root->AddChildToCanvas(SlotsBox);
    ListSlot->SetAnchors(FAnchors(0.5f, 0.32f));
    ListSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    ListSlot->SetSize(FVector2D(760.0f, 300.0f));

    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    UCanvasPanelSlot* ActionsSlot = Root->AddChildToCanvas(Actions);
    ActionsSlot->SetAnchors(FAnchors(0.5f, 0.8f));
    ActionsSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    ActionsSlot->SetSize(FVector2D(760.0f, 60.0f));
    ActionsSlot->SetPosition(FVector2D(-60.0f, 0.0f));

    UWidget* Confirm = nullptr;
    UWidget* Back = nullptr;

    if (ButtonWidgetClass)
    {
        // Create the same Blueprint button used by the main menu.
        StyledConfirmButton = CreateWidget<UButtonWithText>(this, ButtonWidgetClass);
        StyledBackButton = CreateWidget<UButtonWithText>(this, ButtonWidgetClass);
        Confirm = StyledConfirmButton.Get();
        Back = StyledBackButton.Get();
    }
    else
    {
        ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
        ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmText"));
        ConfirmButton->AddChild(ConfirmText);
        Confirm = ConfirmButton.Get();

        BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackButton"));
        UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>();
        BackText->SetText(NSLOCTEXT("MazeSaveMenu", "Back", "Back"));
        BackButton->AddChild(BackText);
        Back = BackButton.Get();
    }

    if (Confirm)
    {
        UHorizontalBoxSlot* ConfirmSlot = Actions->AddChildToHorizontalBox(Confirm);

        // Give the longer save text more room and leave a gap before Back.
        FSlateChildSize ConfirmSize(ESlateSizeRule::Fill);
        ConfirmSize.Value = 3.0f;
        ConfirmSlot->SetSize(ConfirmSize);
        ConfirmSlot->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
        Confirm->SetRenderTranslation(FVector2D(70.0f, 0.0f));
    }

    if (Back)
    {
        UHorizontalBoxSlot* BackSlot = Actions->AddChildToHorizontalBox(Back);
        BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        BackSlot->SetPadding(FMargin(24.0f, 0.0f, 0.0f, 0.0f));
    }
}

void UMazeSaveMenu::NativeConstruct()
{
    Super::NativeConstruct();
    SaveSystem = UMazeSaveSubsystem::Get(this);

    const bool bHasConfirm = StyledConfirmButton || (ConfirmButton && ConfirmText);
    const bool bHasBack = StyledBackButton || BackButton;

    if (!SaveSystem.IsValid() || !TitleText || !StatusText || !SlotsBox || !bHasConfirm || !bHasBack)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze save menu: Supply TitleText, StatusText, SlotsBox, and styled or plain confirmation/Back buttons. Check ButtonWidgetClass."));
        return;
    }

    // Apply the same heading style to both the save and load menus.
    if (MenuTitleFont.FontObject)
    {
        TitleText->SetFont(MenuTitleFont);
    }

    SaveSystem->OnSaveStarted.AddUniqueDynamic(this, &UMazeSaveMenu::HandleSaveStarted);
    SaveSystem->OnSaveFinished.AddUniqueDynamic(this, &UMazeSaveMenu::HandleSaveFinished);
    SaveSystem->OnSaveListChanged.AddUniqueDynamic(this, &UMazeSaveMenu::RefreshSlots);
    SaveSystem->OnSaveError.AddUniqueDynamic(this, &UMazeSaveMenu::HandleSaveError);
    if (StyledConfirmButton)
    {
        StyledConfirmButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMazeSaveMenu::ConfirmSelection);
    }
    else
    {
        ConfirmButton->OnClicked.AddUniqueDynamic(this, &UMazeSaveMenu::ConfirmSelection);
    }

    if (StyledBackButton)
    {
        StyledBackButton->SetText(NSLOCTEXT("MazeSaveMenu", "Back", "Back"));
        StyledBackButton->OnBackgroundButtonClicked.AddUniqueDynamic(this, &UMazeSaveMenu::CloseSaveMenu);
    }
    else
    {
        BackButton->OnClicked.AddUniqueDynamic(this, &UMazeSaveMenu::CloseSaveMenu);
    }
    TitleText->SetText(MenuMode == EMazeSaveMenuMode::Save ? NSLOCTEXT("MazeSaveMenu", "SaveTitle", "Save Game") : NSLOCTEXT("MazeSaveMenu", "LoadTitle", "Load Game"));
    StatusText->SetText(FText::GetEmpty());
    RefreshSlots();
}

void UMazeSaveMenu::ClearSlotBindings()
{
    for (UMazeSaveSlotBinding* Binding : SlotBindings)
    {
        if (IsValid(Binding))
        {
            Binding->ClearBinding();
        }
    }

    SlotBindings.Reset();
    SlotWidgets.Reset();
}

void UMazeSaveMenu::RefreshSlots()
{
    if (!SaveSystem.IsValid() || !SlotsBox)
    {
        return;
    }

    const TArray<FMazeSaveSummary> Summaries = SaveSystem->GetSaveSlots();

    // Selection and save messages update the existing rows instead of recreating focused buttons.
    if (SlotWidgets.Num() != Summaries.Num())
    {
        ClearSlotBindings();
        SlotsBox->ClearChildren();

        for (const FMazeSaveSummary& Summary : Summaries)
        {
            UWidget* SlotWidget = nullptr;

            if (ButtonWidgetClass)
            {
                UButtonWithText* Button = CreateWidget<UButtonWithText>(this, ButtonWidgetClass);

                if (!IsValid(Button))
                {
                    UE_LOG(LogTemp, Error, TEXT("Maze save menu: Could not create the configured ButtonWidgetClass."));
                    return;
                }

                UMazeSaveSlotBinding* Binding = NewObject<UMazeSaveSlotBinding>(this);
                Binding->SetupSlot(this, Summary.SlotIndex, Button);
                SlotBindings.Add(Binding);
                SlotWidget = Button;
            }
            else
            {
                UMazeSaveSlotButton* Button = WidgetTree->ConstructWidget<UMazeSaveSlotButton>();
                Button->SetupSlot(this, Summary.SlotIndex);
                UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
                Text->SetJustification(ETextJustify::Center);
                Button->AddChild(Text);
                SlotWidget = Button;
            }

            SlotWidgets.Add(SlotWidget);
            USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
            Height->SetHeightOverride(FMath::Max(24.0f, SlotButtonHeight));
            Height->AddChild(SlotWidget);
            UVerticalBoxSlot* Row = SlotsBox->AddChildToVerticalBox(Height);
            Row->SetPadding(FMargin(0.0f, 4.0f));
        }
    }

    for (int32 Index = 0; Index < Summaries.Num(); ++Index)
    {
        const FMazeSaveSummary& Summary = Summaries[Index];
        UWidget* SlotWidget = SlotWidgets[Index].Get();
        SlotWidget->SetIsEnabled(!SaveSystem->IsBusy() && (MenuMode == EMazeSaveMenuMode::Save || Summary.bHasSave));

        FString Label = FString::Printf(TEXT("Slot %d - Empty"), Summary.SlotIndex + 1);

        if (Summary.bHasSave)
        {
            const TCHAR* Difficulty = Summary.Difficulty == EMazeDifficulty::Easy ? TEXT("Easy") : TEXT("Hard");
            Label = FString::Printf(TEXT("Slot %d - %s - %d/3 puzzles\nSaved: %s"), Summary.SlotIndex + 1, Difficulty, Summary.MazesCompleted, *Summary.SavedAt.ToString(TEXT("%m/%d/%Y")));

            if (Summary.Difficulty == EMazeDifficulty::Hard)
            {
                Label += FString::Printf(TEXT(" - Second Chance: %d"), Summary.SecondChanceCharges);
            }
        }
        else if (Summary.bSlotOccupied)
        {
            Label = FString::Printf(TEXT("Slot %d - Unavailable save"), Summary.SlotIndex + 1);
        }

        if (UButtonWithText* Button = Cast<UButtonWithText>(SlotWidget))
        {
            Button->SetText(FText::FromString(Label));
        }
        else if (UMazeSaveSlotButton* PlainButton = Cast<UMazeSaveSlotButton>(SlotWidget))
        {
            PlainButton->SetBackgroundColor(Summary.SlotIndex == SelectedSlotIndex ? FLinearColor(0.2f, 0.4f, 0.65f) : FLinearColor(0.2f, 0.2f, 0.2f));

            if (UTextBlock* Text = Cast<UTextBlock>(PlainButton->GetContent()))
            {
                Text->SetText(FText::FromString(Label));
            }
        }
    }

    RefreshControls();
}

void UMazeSaveMenu::RefreshControls()
{
    UWidget* Confirm = StyledConfirmButton ? static_cast<UWidget*>(StyledConfirmButton.Get()) : ConfirmButton.Get();
    UWidget* Back = StyledBackButton ? static_cast<UWidget*>(StyledBackButton.Get()) : BackButton.Get();

    if (!SaveSystem.IsValid() || !Confirm || (!StyledConfirmButton && !ConfirmText) || !Back)
    {
        return;
    }

    const FMazeSaveSummary Selected = SaveSystem->GetSlotSummary(SelectedSlotIndex);
    const bool bSelected = Selected.SlotIndex != INDEX_NONE;
    Confirm->SetIsEnabled(!SaveSystem->IsBusy() && bSelected && (MenuMode == EMazeSaveMenuMode::Save || Selected.bHasSave));
    Back->SetIsEnabled(!SaveSystem->IsBusy());

    FString Action = MenuMode == EMazeSaveMenuMode::Save ? TEXT("Select a slot to save") : TEXT("Select a save to load");

    if (bSelected)
    {
        const TCHAR* Verb = MenuMode == EMazeSaveMenuMode::Load ? TEXT("Load") : (Selected.bSlotOccupied ? TEXT("Overwrite") : TEXT("Save to"));
        Action = FString::Printf(TEXT("%s Slot %d"), Verb, SelectedSlotIndex + 1);
    }

    if (StyledConfirmButton)
    {
        StyledConfirmButton->SetText(FText::FromString(Action));
    }
    else
    {
        ConfirmText->SetText(FText::FromString(Action));
    }
}

void UMazeSaveMenu::SelectSlot(int32 SlotIndex)
{
    if (!SaveSystem.IsValid() || SaveSystem->IsBusy())
    {
        return;
    }

    const FMazeSaveSummary Summary = SaveSystem->GetSlotSummary(SlotIndex);

    if (Summary.SlotIndex == INDEX_NONE || (MenuMode == EMazeSaveMenuMode::Load && !Summary.bHasSave))
    {
        return;
    }

    SelectedSlotIndex = SlotIndex;
    RefreshSlots();
}

void UMazeSaveMenu::ConfirmSelection()
{
    if (!SaveSystem.IsValid() || SaveSystem->IsBusy())
    {
        return;
    }

    if (MenuMode == EMazeSaveMenuMode::Save)
    {
        // Choosing a row only selects it. This separate button confirms the overwrite.
        SaveSystem->SaveToSlot(SavePlayer.Get(), SelectedSlotIndex, true);
    }
    else
    {
        SaveSystem->LoadSaveSlot(SelectedSlotIndex);
    }
}

void UMazeSaveMenu::HandleSaveStarted(int32 SlotIndex)
{
    if (StatusText)
    {
        StatusText->SetText(NSLOCTEXT("MazeSaveMenu", "Saving", "Saving..."));
    }

    RefreshSlots();
}

void UMazeSaveMenu::HandleSaveFinished(bool bSuccess)
{
    if (StatusText)
    {
        StatusText->SetText(bSuccess ? NSLOCTEXT("MazeSaveMenu", "Saved", "Saved") : NSLOCTEXT("MazeSaveMenu", "SaveFailed", "Save failed. Try again."));
    }

    RefreshSlots();
}

void UMazeSaveMenu::HandleSaveError(const FString& Message)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Message));
    }
}

void UMazeSaveMenu::ReleasePlayerInput()
{
    if (!MenuController.IsValid())
    {
        return;
    }

    if (bLockedPlayerInput)
    {
        // Balance only this menu's locks so another system's input lock is preserved.
        MenuController->SetIgnoreMoveInput(false);
        MenuController->SetIgnoreLookInput(false);
        bLockedPlayerInput = false;
    }

    if (!SaveSystem.IsValid() || !SaveSystem->IsBusy())
    {
        if (MenuMode == EMazeSaveMenuMode::Save)
        {
            MenuController->SetInputMode(FInputModeGameOnly());
        }
        else
        {
            MenuController->SetInputMode(FInputModeUIOnly());
        }

        MenuController->bShowMouseCursor = bPreviousMouseCursor;
    }
}

void UMazeSaveMenu::CloseSaveMenu()
{
    if (SaveSystem.IsValid() && SaveSystem->IsBusy())
    {
        return;
    }

    RemoveFromParent();
}

void UMazeSaveMenu::NativeDestruct()
{
    if (SaveSystem.IsValid())
    {
        SaveSystem->OnSaveStarted.RemoveDynamic(this, &UMazeSaveMenu::HandleSaveStarted);
        SaveSystem->OnSaveFinished.RemoveDynamic(this, &UMazeSaveMenu::HandleSaveFinished);
        SaveSystem->OnSaveListChanged.RemoveDynamic(this, &UMazeSaveMenu::RefreshSlots);
        SaveSystem->OnSaveError.RemoveDynamic(this, &UMazeSaveMenu::HandleSaveError);
    }

    if (ConfirmButton)
    {
        ConfirmButton->OnClicked.RemoveDynamic(this, &UMazeSaveMenu::ConfirmSelection);
    }

    if (BackButton)
    {
        BackButton->OnClicked.RemoveDynamic(this, &UMazeSaveMenu::CloseSaveMenu);
    }

    if (StyledConfirmButton)
    {
        StyledConfirmButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMazeSaveMenu::ConfirmSelection);
    }

    if (StyledBackButton)
    {
        StyledBackButton->OnBackgroundButtonClicked.RemoveDynamic(this, &UMazeSaveMenu::CloseSaveMenu);
    }

    ClearSlotBindings();
    ReleasePlayerInput();
    Super::NativeDestruct();
}