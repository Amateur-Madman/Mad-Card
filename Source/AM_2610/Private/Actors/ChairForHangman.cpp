// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChairForHangman.h"
#include "Actors/BasePlayer.h"
#include "Actors/WrittingBoard.h"
#include "EngineUtils.h" 

AChairForHangman::AChairForHangman()
{
	bRequiresInteract = true;
}

void AChairForHangman::HandlePickup(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if (ABasePlayer* OtherCharacter = Cast<ABasePlayer>(OtherActor))
		{
            CaughtPlayer = OtherCharacter;
            CaughtPlayer->SetActorLocation(ChildActorComponent->GetComponentLocation());
            if(WrittingBoard)
            {
                OtherCharacter->BeginViewLock(WrittingBoard, FVector(0.0f, 0.0f, 0.0f));
			}
		}
	}
}

void AChairForHangman::BeginPlay()
{
	Super::BeginPlay();

	FindWrittingBoard();
}

void AChairForHangman::PostPickup()
{
    //Empty; Do nothing when "picked up"
}

void AChairForHangman::FindWrittingBoard()
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AWrittingBoard> It(World); It; ++It)
        {
            WrittingBoard = *It;

            if (WrittingBoard)
            {
                UE_LOG(LogTemp, Log, TEXT("Found object name: %s"), *WrittingBoard->GetName());
            }
        }
    }
}
