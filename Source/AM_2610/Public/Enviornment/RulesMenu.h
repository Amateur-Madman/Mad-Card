// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "RulesMenu.generated.h"

class UImage;
class UTextBlock;
class UVerticalBox;
class URulesSubsystem;

UCLASS()
class AM_2610_API URulesMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    URulesMenu(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Rules")
    void CloseRulesMenu();

    void SetRulesSystem(URulesSubsystem* System);
    void PrepareCloseInput(bool bOpeningKeyDown);
    bool HasRulesLayout() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // Edit these in the Widget Blueprint's Class Defaults.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
    FText Heading;

    // Each section starts with a heading. Leave a blank line between sections.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules", meta = (MultiLine = "true"))
    FText Rules;

    // Copy the Load Game title font here. Change Size to resize the main heading.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Style", meta = (DisplayName = "Title Font"))
    FSlateFontInfo MenuTitleFont;

    // Use the font from the existing menu buttons for the rules and close hint.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Style", meta = (DisplayName = "Body Font"))
    FSlateFontInfo MenuBodyFont;

    // Resize YOUR GOAL, EXPLORING, and the other headings inside the rules text.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Style", meta = (ClampMin = "1", UIMin = "12", UIMax = "48"))
    int32 SectionHeadingSize = 26;

    // Keep the section headings brighter than their explanations.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Style")
    FLinearColor SectionHeadingColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Copy the Brush from the save button's Img_Selector.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Style")
    FSlateBrush SelectorBrush;

    // Leave the Designer empty for the built-in layout, or use these exact widget names.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RulesTitle;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> RulesText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CloseHint;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Img_Selector;

private:
    TWeakObjectPtr<URulesSubsystem> RulesSystem;
    bool bWaitForOpeningERelease = false;
    bool bCloseOnERelease = false;

    void BuildDefaultLayout();
    void BuildRulesSections();
};