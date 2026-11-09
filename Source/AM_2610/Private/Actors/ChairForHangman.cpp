// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChairForHangman.h"
#include "Actors/BasePlayer.h"
#include "Actors/BasePlayerController.h"
#include "Actors/WrittingBoard.h"
#include "EngineUtils.h"
#include "Both/CharacterAnimation.h" 
#include "GameFramework/GameStateBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AChairForHangman::AChairForHangman()
{
	bRequiresInteract = true;

    ChildActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ChildActorComponent"));
    if (ChildActorComponent)
    {
        ChildActorComponent->SetupAttachment(GetRootComponent());
    }
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
                if(UCharacterAnimation* OtherCharacterAnim = Cast<UCharacterAnimation>(OtherCharacter->GetMesh()->GetAnimInstance()))
                {
                    OtherCharacterAnim->SitAnimation();
                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimer(SitEndedTimerHandle, this, &AChairForHangman::HandlePlayerReady, 2.23f, false);
                    }
				}
			}
			BoxCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void AChairForHangman::BeginPlay()
{
	Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            if (ABasePlayerController* PC = Cast<ABasePlayerController>(GameState->GetGameInstance()->GetFirstLocalPlayerController(World)))
            {
                if (ABasePlayer* MyChar = Cast<ABasePlayer>(PC->GetPawn()))
                {
                    MyChar->RegisterChair(this);
                }
            }
        }
    }
}

void AChairForHangman::PostPickup()
{
    //Empty; Do nothing after "picked up"
}

void AChairForHangman::HandlePlayerReady()
{
    if (CaughtPlayer)
    {
        OnPlayerReady.Broadcast();
	}
}