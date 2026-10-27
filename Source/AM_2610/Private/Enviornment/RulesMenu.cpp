// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/RulesMenu.h"
#include "Enviornment/RulesSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

URulesMenu::URulesMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    MenuTitleFont.Size = 72;
    MenuTitleFont.TypefaceFontName = FName(TEXT("Default"));
    MenuBodyFont.Size = 22;
    MenuBodyFont.TypefaceFontName = FName(TEXT("Default"));

    Heading = NSLOCTEXT("RulesMenu", "Heading", "HOW TO SURVIVE");
    Rules = NSLOCTEXT("RulesMenu", "Rules",
        "OBJECTIVE\n"
        "To get out of here, you'll need to complete three different maze puzzles. Solve the puzzle in each maze, then find your way to the exit. Escape all three mazes to win.\n\n"
        "EXPLORING\n"
        "Choose a door in the start room, and a new maze will form beyond it. Search for the puzzle, solve it, then find the end door to reach the next start room. Check the rooms along the way. Some have cards to help you survive, while others have traps waiting for you.\n\n"
        "SURVIVING\n"
        "Keep your eyes on the Weeping Angel. Looking at it stops it from moving, but the moment you look away, it can come after you again. Watch the ceiling, too. Walking directly beneath a HangMan triggers its attack. If the Chaser is behind you, keep moving. You don't want to let him catch up.\n\n"
        "CARDS\n"
        "Face a card and press E to collect it when your hand has space. Use the controls shown on your HUD to play your cards. Dash gives you a burst of speed and lets you pass through objects while it's active. Decoy places a little buddy that draws enemies away from you. Second Chance is stored as a separate charge on your HUD, so it won't take up space in your hand.\n\n"
        "RETRYING AND SAVING\n"
        "When you retry, you'll return to your start room checkpoint and keep your completed maze progress. The unfinished maze will be cleared, so you'll need to choose a door again. If you need a break, use the save table in a start room. Choose a save slot, then load it from the main menu when you're ready to return.\n\n"
        "READ THESE AGAIN\n"
        "If you need a reminder later, return to a rules terminal, face its screen, and press E to read these rules again.");
}

void URulesMenu::SetRulesSystem(URulesSubsystem* System)
{
    RulesSystem = System;
}

void URulesMenu::PrepareCloseInput(bool bOpeningKeyDown)
{
    // The E press that opens the terminal must finish before another press can close it.
    bWaitForOpeningERelease = bOpeningKeyDown;
    bCloseOnERelease = false;
}

bool URulesMenu::HasRulesLayout() const
{
    return IsValid(RulesTitle) && IsValid(RulesText);
}

TSharedRef<SWidget> URulesMenu::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    if (!WidgetTree->RootWidget)
    {
        BuildDefaultLayout();
    }

    return Super::RebuildWidget();
}

void URulesMenu::BuildDefaultLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RulesCanvas"));
    WidgetTree->RootWidget = Root;

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    // Match the full-screen background used by the save menu.
    Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.96f));
    UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackgroundSlot->SetOffsets(FMargin(0.0f));

    RulesTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RulesTitle"));
    FSlateFontInfo TitleFont = RulesTitle->GetFont();
    TitleFont.Size = 72;
    TitleFont.TypefaceFontName = FName(TEXT("Regular"));
    RulesTitle->SetFont(TitleFont);
    RulesTitle->SetJustification(ETextJustify::Center);
    RulesTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.91f)));
    UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(RulesTitle);
    TitleSlot->SetAnchors(FAnchors(0.5f, 0.08f));
    TitleSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    TitleSlot->SetAutoSize(true);

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    UCanvasPanelSlot* ScrollSlot = Root->AddChildToCanvas(Scroll);
    ScrollSlot->SetAnchors(FAnchors(0.22f, 0.3f, 0.78f, 0.8f));
    ScrollSlot->SetOffsets(FMargin(0.0f));

    RulesText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RulesText"));
    Scroll->AddChild(RulesText);

    FSlateFontInfo BodyFont = RulesTitle->GetFont();
    BodyFont.Size = 22;

    UVerticalBox* Footer = WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* FooterSlot = Root->AddChildToCanvas(Footer);
    FooterSlot->SetAnchors(FAnchors(0.5f, 0.86f));
    FooterSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    FooterSlot->SetAutoSize(true);

    CloseHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseHint"));
    FSlateFontInfo HintFont = BodyFont;
    HintFont.Size = 26;
    CloseHint->SetFont(HintFont);
    CloseHint->SetJustification(ETextJustify::Center);
    CloseHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.91f)));
    UVerticalBoxSlot* HintSlot = Footer->AddChildToVerticalBox(CloseHint);
    HintSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    // Use the same selector image as the other menus.
    USizeBox* SelectorBox = WidgetTree->ConstructWidget<USizeBox>();
    SelectorBox->SetWidthOverride(260.0f);
    SelectorBox->SetHeightOverride(20.0f);
    UVerticalBoxSlot* SelectorSlot = Footer->AddChildToVerticalBox(SelectorBox);
    SelectorSlot->SetHorizontalAlignment(HAlign_Center);
    Img_Selector = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Selector"));
    Img_Selector->SetVisibility(ESlateVisibility::Hidden);
    SelectorBox->AddChild(Img_Selector);
}

void URulesMenu::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (RulesTitle)
    {
        // Size still applies when the font family uses the built-in fallback.
        FSlateFontInfo TitleFont = MenuTitleFont.FontObject ? MenuTitleFont : RulesTitle->GetFont();
        TitleFont.Size = MenuTitleFont.Size;
        RulesTitle->SetFont(TitleFont);
        RulesTitle->SetText(Heading);
    }

    if (RulesText)
    {
        BuildRulesSections();
    }

    if (CloseHint)
    {
        if (MenuBodyFont.FontObject)
        {
            FSlateFontInfo HintFont = MenuBodyFont;
            HintFont.Size = 26;
            CloseHint->SetFont(HintFont);
        }

        CloseHint->SetText(NSLOCTEXT("RulesMenu", "CloseHint", "Press E to close"));
    }

    if (Img_Selector)
    {
        if (SelectorBrush.GetResourceObject())
        {
            Img_Selector->SetBrush(SelectorBrush);
        }

        // This prompt stays selected. An empty brush stays hidden.
        Img_Selector->SetRenderOpacity(1.0f);
        Img_Selector->SetVisibility(Img_Selector->GetBrush().GetResourceObject() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

void URulesMenu::BuildRulesSections()
{
    RulesText->ClearChildren();

    // Keep editing one Rules field. The first line of each section is its heading.
    FString RulesString = Rules.ToString();
    RulesString.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    RulesString.ReplaceInline(TEXT("\r"), TEXT("\n"));
    TArray<FString> Sections;
    RulesString.ParseIntoArray(Sections, TEXT("\n\n"), true);

    for (const FString& Section : Sections)
    {
        FString SectionHeading;
        FString SectionBody;
        if (!Section.Split(TEXT("\n"), &SectionHeading, &SectionBody))
        {
            SectionBody = Section;
        }

        SectionHeading.TrimStartAndEndInline();
        SectionBody.TrimStartAndEndInline();

        UTextBlock* BodyText = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo BodyFont = BodyText->GetFont();
        BodyFont.TypefaceFontName = FName(TEXT("Regular"));
        if (MenuBodyFont.FontObject)
        {
            BodyFont = MenuBodyFont;
        }

        BodyFont.Size = MenuBodyFont.Size;

        if (!SectionHeading.IsEmpty())
        {
            UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>();
            // Keep the body font and only change the heading's size.
            FSlateFontInfo HeaderFont = BodyFont;
            HeaderFont.Size = FMath::Max(1, SectionHeadingSize);
            HeaderText->SetFont(HeaderFont);
            HeaderText->SetText(FText::FromString(SectionHeading));
            HeaderText->SetAutoWrapText(true);
            HeaderText->SetColorAndOpacity(FSlateColor(SectionHeadingColor));
            UVerticalBoxSlot* HeaderSlot = RulesText->AddChildToVerticalBox(HeaderText);
            HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
        }

        BodyText->SetFont(BodyFont);
        BodyText->SetText(FText::FromString(SectionBody));
        BodyText->SetAutoWrapText(true);
        BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.83f, 0.81f)));
        UVerticalBoxSlot* BodySlot = RulesText->AddChildToVerticalBox(BodyText);
        BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
    }
}

void URulesMenu::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
}

FReply URulesMenu::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::E)
    {
        if (!bWaitForOpeningERelease && !InKeyEvent.IsRepeat())
        {
            bCloseOnERelease = true;
        }

        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply URulesMenu::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::E)
    {
        if (bWaitForOpeningERelease)
        {
            bWaitForOpeningERelease = false;
            bCloseOnERelease = false;
        }
        else if (bCloseOnERelease)
        {
            // Return to gameplay after E is released so this press cannot reopen the terminal.
            bCloseOnERelease = false;
            CloseRulesMenu();
        }

        return FReply::Handled();
    }

    return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void URulesMenu::CloseRulesMenu()
{
    if (RulesSystem.IsValid())
    {
        RulesSystem->CloseRules();
    }
    else
    {
        RemoveFromParent();
    }
}

void URulesMenu::NativeDestruct()
{
    if (RulesSystem.IsValid())
    {
        RulesSystem->NotifyMenuRemoved(this);
    }

    RulesSystem.Reset();
    bWaitForOpeningERelease = false;
    bCloseOnERelease = false;
    Super::NativeDestruct();
}