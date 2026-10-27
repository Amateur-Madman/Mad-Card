// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utility/Interactable.h"
#include "MazeSaveTable.generated.h"

class AMazeActors;
class UBoxComponent;
class UStaticMeshComponent;
class UMazeSaveMenu;

UCLASS()
class AM_2610_API AMazeSaveTable : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AMazeSaveTable();

    virtual bool CanInteract_Implementation(AActor* Interactor) override;
    virtual FInteractPrompt GetInteractPrompt_Implementation(AActor* Interactor) override;
    virtual void Interact_Implementation(AActor* Interactor) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|Save")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|Save")
    TObjectPtr<UStaticMeshComponent> TableMesh;

    // Child-actor tables find their parent room. Set this only for a separately placed table.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Maze|Save")
    TObjectPtr<AMazeActors> SaveRoom = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Save")
    EInteractSlot InteractSlot = EInteractSlot::Primary;

    // Leave empty to use the built-in list, or assign a styled child Widget Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Save")
    TSubclassOf<UMazeSaveMenu> SaveMenuClass;

private:
    AMazeActors* FindSaveRoom() const;
};
