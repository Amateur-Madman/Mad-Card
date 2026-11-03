#include "Actors/ScanPulse.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Utility/ScannableInterface.h"

AScanPulse::AScanPulse()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HumComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("HumComponent"));
	HumComponent->SetupAttachment(Root);
	HumComponent->bAutoActivate = false;
}

void AScanPulse::BeginPlay()
{
	Super::BeginPlay();

	const FVector Origin = GetActorLocation();

	TArray<AActor*> Scannables;
	UGameplayStatics::GetAllActorsWithInterface(this, UScannableInterface::StaticClass(), Scannables);

	for (AActor* Target : Scannables)
	{
		const float Distance = FVector::Dist(Origin, Target->GetActorLocation());
		if (Distance > MaxRadius)
		{
			continue;
		}

		// The wave "arrives" after distance / speed. Rate must be > 0 or SetTimer clears instead.
		const float Delay = FMath::Max(Distance / Speed, 0.01f);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateUObject(this, &AScanPulse::RevealActor, TWeakObjectPtr<AActor>(Target)),
			Delay,
			false);
	}

	const float SweepTime = MaxRadius / Speed;
	HumComponent->Play();
	GetWorldTimerManager().SetTimer(EndSweepTimer, this, &AScanPulse::EndSweep, SweepTime, false);

	// Outlives the last possible reveal timer and the hum fade, then cleans itself up
	SetLifeSpan(SweepTime + HumFadeOutTime + 0.1f);
}

void AScanPulse::RevealActor(TWeakObjectPtr<AActor> Target)
{
	AActor* Actor = Target.Get();
	if (!IsValid(Actor))
	{
		return; // enemy was destroyed while the wave was travelling
	}

	IScannableInterface::Execute_ScanRevealed(Actor, RevealDuration);

	if (PingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PingSound, Actor->GetActorLocation());
	}
}

void AScanPulse::EndSweep()
{
	HumComponent->FadeOut(HumFadeOutTime, 0.0f);
}