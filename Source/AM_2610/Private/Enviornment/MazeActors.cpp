// Fill out your copyright notice in the Description page of Project Settings.


#include "Enviornment/MazeActors.h"
#include "Components/ArrowComponent.h"

// Sets default values
AMazeActors::AMazeActors()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

TArray<UArrowComponent*> AMazeActors::GetConnections() const
{
    TArray<UArrowComponent*> Arrows;
    GetComponents<UArrowComponent>(Arrows);

    TArray<UArrowComponent*> Connections;

    for (UArrowComponent* Arrow : Arrows)
    {
        if (IsValid(Arrow) && Arrow->ComponentHasTag(FName(TEXT("MazeConnection"))))
        {
            Connections.Add(Arrow);
        }
    }

    return Connections;
}

// Let AI check the player's room area before choosing its behaviour.
bool AMazeActors::IsPlayerInRoom(AActor* Player)
{
    if (!IsValid(Player))
    {
        return false;
    }

    TArray<UPrimitiveComponent*> Overlaps;
    Player->GetOverlappingComponents(Overlaps);

    for (UPrimitiveComponent* Component : Overlaps)
    {
        // Only tagged triggers on maze pieces count as rooms.
        if (IsValid(Component) && Component->ComponentHasTag(TEXT("RoomArea")) && IsValid(Cast<AMazeActors>(Component->GetOwner())))
        {
            return true;
        }
    }

    return false;
}