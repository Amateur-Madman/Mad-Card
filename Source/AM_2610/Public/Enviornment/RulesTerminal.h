// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utility/Interactable.h"
#include "RulesTerminal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class URulesMenu;

// The existing player interaction system supplies the E input and HUD prompt.
UCLASS()
class AM_2610_API ARulesTerminal : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ARulesTerminal();

    virtual bool CanInteract_Implementation(AActor* Interactor) override;
    virtual FInteractPrompt GetInteractPrompt_Implementation(AActor* Interactor) override;
    virtual void Interact_Implementation(AActor* Interactor) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rules")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rules")
    TObjectPtr<UStaticMeshComponent> TerminalMesh;

    // The subsystem accepts one startup request per level, even with several terminals.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
    bool bShowRulesOnLevelLoad = true;

    // Assign the same Widget Blueprint to every terminal. Empty uses the built-in layout.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
    TSubclassOf<URulesMenu> RulesMenuClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
    EInteractSlot InteractSlot = EInteractSlot::Primary;

protected:
    virtual void BeginPlay() override;
};