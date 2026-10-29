// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Projectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../AM_2610.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AProjectile::AProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereCollision = CreateDefaultSubobject<USphereComponent>("SphereCollision");
	//Just like dragging the component into the root component in the editor
	SetRootComponent(SphereCollision);
	SphereCollision->OnComponentHit.AddDynamic(this, &AProjectile::HandleHit);
	//declare mesh and attach as child
	static ConstructorHelpers::FObjectFinder<UStaticMesh>Asset(TEXT("StaticMesh '/Engine/BasicShapes/Sphere.Sphere'"));

	SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>("SphereMesh");
	SphereMesh->SetCollisionProfileName("NoCollision");
	SphereMesh->SetupAttachment(SphereCollision);
	SphereMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
	SphereMesh->SetStaticMesh(Asset.Object);

	//setup projectile movement component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->InitialSpeed = 1900.0f;
	ProjectileMovement->MaxSpeed = 1900.0f;

	//scale the whole actor (root) using the same Size vector driving the collision radius
	SetActorScale3D(Size);
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	FTimerHandle ProjectileTimer;
	//change number 3 to variable
	GetWorld()->GetTimerManager().SetTimer(ProjectileTimer, this, &AProjectile::K2_DestroyActor,DestroyTime);
	
}

// Called every frame
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor != GetInstigator())
	{
		UGameplayStatics::ApplyDamage(OtherActor, 10.0f, GetInstigatorController(), this, nullptr);
	}
	UE_LOG(LogTemp, Log, TEXT("Destroyed"));
	Destroy();	Destroy();
	
}

