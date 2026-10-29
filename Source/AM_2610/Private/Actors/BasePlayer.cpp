// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/BasePlayer.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Utility/EnemyBehaviorInterface.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Actors/Rifle.h"
#include "Both/CharacterAnimation.h"
#include "Both/PlayerHUD.h"
#include "Utility/HealthComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Cards/CardHandComponent.h"
#include "Audio/PlayerAudioComponent.h"
#include "Utility/AITargetStack.h"
#include "../AM_2610.h"



ABasePlayer::ABasePlayer()
{

	// Only ticks while a view lock (grapple) is active - see BeginViewLock
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Character rotation
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// First-person camera: attached straight to the capsule at eye height (no spring arm).
	// Tune the Z offset in BP_BasePlayer's viewport to match the capsule / mesh height.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));

	// With no spring arm, the camera itself reads the controller's pitch/yaw
	Camera->bUsePawnControlRotation = true;

	// The full body mesh would clip through the camera, so hide it from the owning player
	// (it still renders for everyone else). First-person arms get added later if wanted.
	GetMesh()->SetOwnerNoSee(true);

	// Interaction sphere - proximity detection only.
	// Object type Pawn so pickups (which overlap Pawn) can see it; it ignores everything
	// except the "Pickup" channel, so it never interacts with enemies, projectiles, etc.
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetRootComponent());
	InteractionSphere->InitSphereRadius(InteractRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_Pawn);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // "Pickup"
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->SetCanEverAffectNavigation(false);

	// Set the soft path in code (still no hardcoded FClassFinder / synchronous load)
	HUDClass = TSoftClassPtr<UPlayerHUD>(FSoftObjectPath(TEXT("/Game/Both/WBP_PlayerHUD_New.WBP_PlayerHUD_C")));

	CardHand = CreateDefaultSubobject<UCardHandComponent>(TEXT("CardHand"));
	PlayerAudio = CreateDefaultSubobject<UPlayerAudioComponent>(TEXT("PlayerAudio"));

	// Look detection: sight-only perception on the player.
	CameraPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("CameraPerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	if (SightConfig && CameraPerceptionComponent)
	{
		SightConfig->SightRadius = 5000.f;                // max distance an enemy can be noticed from
		SightConfig->LoseSightRadius = 5500.f;
		SightConfig->PeripheralVisionAngleDegrees = 45.f; // half-angle: 45 left + 45 right = 90 degree cone (match the camera FOV)

		// Agents implement IGenericTeamAgentInterface (same TeamNumber = friendly), so keep all three on
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

		CameraPerceptionComponent->ConfigureSense(*SightConfig);
		CameraPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	//Set player's team
	TeamNumber = 1;
}
void ABasePlayer::Move(const FInputActionValue& Value)
{
	// X = strafe (A/D), Y = forward (W/S) - the IMC's Swizzle/Negate modifiers shape this
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);

	AddMovementInput(YawRotation.Vector(), Axis.Y);
	AddMovementInput(YawRotation.RotateVector(FVector::RightVector), Axis.X);
}

void ABasePlayer::Look(const FInputActionValue& Value)
{
	// X = yaw, Y = pitch (the IMC negates mouse Y so pushing the mouse up looks up)
	const FVector2D Axis = Value.Get<FVector2D>();

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

FVector ABasePlayer::GetCrosshairTarget() const
{
	if (HUDObject)
	{
		return HUDObject->GetDestination();
	}
	// HUD not ready yet - fall back to straight down the camera's forward vector
	return Camera->GetComponentLocation() + (Camera->GetForwardVector() * 10000.f);
}

void ABasePlayer::OnPlayerSpottedActor(AActor* Actor, FAIStimulus Stimulus)
{
	if (bIsDead || !Actor || !Actor->Implements<UEnemyBehaviorInterface>())
	{
		return;
	}

	// WasSuccessfullySensed(): true when the actor enters the player's view, false when it leaves
	IEnemyBehaviorInterface::Execute_PlayerSeesMe(Actor, Stimulus.WasSuccessfullySensed());
	if (PlayerAudio && Stimulus.WasSuccessfullySensed()) { PlayerAudio->PlayEnemySpotted(); }
}

void ABasePlayer::BeginViewLock(AActor* Target, FVector TargetOffset, float InterpSpeed)
{
	if (bIsDead || !IsValid(Target))
	{
		return;
	}

	ViewLockTarget = Target;
	ViewLockOffset = TargetOffset;
	ViewLockSpeed = InterpSpeed;

	if (!bViewLocked)
	{
		bViewLocked = true;
		if (ThePlayerController)
		{
			ThePlayerController->SetIgnoreLookInput(true);
			ThePlayerController->SetIgnoreMoveInput(true);
		}
		SetActorTickEnabled(true);
	}
}

void ABasePlayer::EndViewLock()
{
	if (!bViewLocked)
	{
		return;
	}

	bViewLocked = false;
	ViewLockTarget.Reset();
	if (ThePlayerController)
	{
		ThePlayerController->SetIgnoreLookInput(false);
		ThePlayerController->SetIgnoreMoveInput(false);
	}
	SetActorTickEnabled(false);
}

void ABasePlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bViewLocked)
	{
		return;
	}

	AActor* Target = ViewLockTarget.Get();
	if (!IsValid(Target) || !ThePlayerController)
	{
		EndViewLock(); // enemy died or despawned mid-grapple
		return;
	}

	const FVector AimPoint = Target->GetActorLocation() + ViewLockOffset;
	const FRotator Desired = (AimPoint - Camera->GetComponentLocation()).Rotation();
	ThePlayerController->SetControlRotation(
		FMath::RInterpTo(ThePlayerController->GetControlRotation(), Desired, DeltaTime, ViewLockSpeed));
}

void ABasePlayer::BeginPlay()
{
	Super::BeginPlay();

	// Look detection (before the HUD early-returns below, so it works even if the HUD fails)
	if (CameraPerceptionComponent)
	{
		CameraPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ABasePlayer::OnPlayerSpottedActor);
	}

	// ---- Interaction setup (before the HUD early-returns below, so it works even if the HUD fails) ----
	InteractionSphere->SetSphereRadius(InteractRadius); // pick up any Blueprint-side radius override
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABasePlayer::OnInteractionBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ABasePlayer::OnInteractionEndOverlap);
	if (UAITargetStack* Stack = GetWorld()->GetSubsystem<UAITargetStack>())
	{
		Stack->Push(this);
	}

	// Anything already inside the sphere at spawn won't fire a begin-overlap, so seed it manually
	TArray<AActor*> InitialOverlaps;
	InteractionSphere->GetOverlappingActors(InitialOverlaps);
	for (AActor* Actor : InitialOverlaps)
	{
		if (Actor && Actor->Implements<UInteractable>())
		{
			InteractCandidates.AddUnique(Actor);
		}
	}
	if (InteractCandidates.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(FocusTimerHandle, this, &ABasePlayer::UpdateFocus, FocusUpdateInterval, true);
		UpdateFocus();
	}

	ThePlayerController = Cast<APlayerController>(GetController());

	// Register the Enhanced Input mapping context for this local player
	if (ThePlayerController)
	{
		if (ULocalPlayer* LocalPlayer = ThePlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext)
				{
					InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("BeginPlay: DefaultMappingContext is not set on %s - no input will work"), *GetName());
				}
			}
		}
	}

	// AnimationReference is set by Super::BeginPlay() above, so it's safe to bind here
	if (AnimationReference)
	{
		AnimationReference->OnDeathEnded.AddDynamic(this, &ABasePlayer::PlayerLost);
	}
	if (!ThePlayerController || HUDClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginPlay: missing PlayerController or HUDClass - cannot create HUD"));
		return;
	}

	// Resolve the soft class reference (loads it now, only when actually needed)
	UClass* ResolvedHUDClass = HUDClass.LoadSynchronous();
	if (!ResolvedHUDClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginPlay: failed to load HUDClass from soft reference"));
		return;
	}

	HUDObject = CreateWidget<UPlayerHUD>(ThePlayerController, ResolvedHUDClass);
	if (!HUDObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginPlay: failed to create HUDObject"));
		return;
	}
	else HealthComponent->OnHeal.AddDynamic(HUDObject, &UPlayerHUD::SetHealth);

	// HUD formats and shows/hides the "Press [] to {}" text from the prompt struct
	OnInteractPromptChanged.AddDynamic(HUDObject, &UPlayerHUD::SetInteractPrompt);

	HUDObject->AddToViewport();

	if (CardHand)
	{
		CardHand->OnCardSlotChanged.AddDynamic(HUDObject, &UPlayerHUD::SetCardSlot);
		CardHand->OnCardUsed.AddDynamic(HUDObject, &UPlayerHUD::CardUsed);
		CardHand->OnCardRefused.AddDynamic(HUDObject, &UPlayerHUD::CardRefused);
		CardHand->RefreshAll(); // starting cards were added before the HUD existed
	}

}
void ABasePlayer::InputInteract1()
{
	TryInteract(EInteractSlot::Primary);
}

void ABasePlayer::InputInteract2()
{
	TryInteract(EInteractSlot::Secondary);
}
void ABasePlayer::InputUseCard(int32 SlotIndex)
{
	if (bIsDead || !CardHand)
	{
		return;
	}
	CardHand->UseSlot(SlotIndex);
}

void ABasePlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupPlayerInputComponent: not an UEnhancedInputComponent - check Project Settings > Input > Default Classes"));
		return;
	}

	// Look + Move (one Axis2D action each)
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABasePlayer::Look);
	}
	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABasePlayer::Move);
	}

	// Context buttons - Started = once per press
	if (InteractPrimaryAction)
	{
		EnhancedInput->BindAction(InteractPrimaryAction, ETriggerEvent::Started, this, &ABasePlayer::InputInteract1);
	}
	if (InteractSecondaryAction)
	{
		EnhancedInput->BindAction(InteractSecondaryAction, ETriggerEvent::Started, this, &ABasePlayer::InputInteract2);
	}
	// One action per slot; the slot index rides along as the payload
	for (int32 i = 0; i < CardSlotActions.Num(); ++i)
	{
		if (CardSlotActions[i])
		{
			EnhancedInput->BindAction(CardSlotActions[i], ETriggerEvent::Started, this, &ABasePlayer::InputUseCard, i);
		}
	}
}


void ABasePlayer::HandleHurt(float Percent)
{
	Super::HandleHurt(Percent);
	if (PlayerAudio) { PlayerAudio->PlayHurt(); }

	if (HUDObject)
	{
		HUDObject->SetHealth(Percent);
	}
}

void ABasePlayer::HandleDeath(float Percent)
{
	Super::HandleDeath(Percent);
	if (PlayerAudio) { PlayerAudio->PlayDeath(); }
	EndViewLock();
	ClearInteraction();
	if (UAITargetStack* Stack = GetWorld()->GetSubsystem<UAITargetStack>())
	{
		Stack->Remove(this);
	}
	if (HUDObject)
	{
		HUDObject->SetHealth(Percent);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasePlayer::HandleDeath - GetController() did not cast to APlayerController, could not disable input"));
	}
}
void ABasePlayer::PlayerLost()
{
	OnPlayerLost.Broadcast();

	if (ThePlayerController)
	{
		ThePlayerController->bShowMouseCursor = true;
	}

	if (HUDObject)
	{
		HUDObject->RemoveFromParent();
	}
}

void ABasePlayer::PlayerWon()
{
	if (PlayerAudio) { PlayerAudio->PlayWin(); }
	FreezeForResults();
}

void ABasePlayer::FreezeForResults()
{
	EndViewLock();
	ClearInteraction();

	//disable input
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasePlayer::PlayerWon - GetController() did not cast to APlayerController, could not disable input"));
	}
	//remove hud from player
	if (HUDObject)
	{
		HUDObject->RemoveFromParent();
	}
	//set timer to load main menu
}


void ABasePlayer::HandleAmmoChanged(float Current, float Max)
{
	if (HUDObject)
	{
		HUDObject->SetAmmo(Current, Max);
	}
}

void ABasePlayer::AddMaxAmmo(float Amount)
{
	if (WeaponReference)
	{
		WeaponReference->IncreaseMaxAmmo(Amount);
	}
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------
#pragma region Interaction
void ABasePlayer::OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsDead || !OtherActor || !OtherActor->Implements<UInteractable>())
	{
		return;
	}

	InteractCandidates.AddUnique(OtherActor);

	// Focus depends on where the camera is aimed, so re-evaluate on a short timer
	// for as long as anything is in range (no per-frame cost when nothing is near).
	if (!GetWorldTimerManager().IsTimerActive(FocusTimerHandle))
	{
		GetWorldTimerManager().SetTimer(FocusTimerHandle, this, &ABasePlayer::UpdateFocus, FocusUpdateInterval, true);
	}
	UpdateFocus();
}

void ABasePlayer::OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	InteractCandidates.Remove(OtherActor);
	UpdateFocus(); // also stops the timer once nothing is left
}

void ABasePlayer::UpdateFocus()
{
	if (bIsDead)
	{
		ClearInteraction();
		return;
	}

	AActor* Best = nullptr;
	float BestDistance = AimTolerance;

	const FVector AimOrigin = Camera->GetComponentLocation();
	const FVector AimDirection = Camera->GetForwardVector();

	for (int32 i = InteractCandidates.Num() - 1; i >= 0; --i)
	{
		AActor* Candidate = InteractCandidates[i].Get();
		if (!IsValid(Candidate))
		{
			InteractCandidates.RemoveAtSwap(i);
			continue;
		}

		if (!IInteractable::Execute_CanInteract(Candidate, this))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - AimOrigin;
		if (FVector::DotProduct(ToCandidate, AimDirection) <= 0.f)
		{
			continue; // behind the camera
		}

		// Distance from the candidate to the aim ray = "how close to under the crosshair"
		const float Distance = FMath::PointDistToLine(Candidate->GetActorLocation(), AimDirection, AimOrigin);
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}

	SetFocusedInteractable(Best);

	if (InteractCandidates.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(FocusTimerHandle);
	}
}

void ABasePlayer::SetFocusedInteractable(AActor* NewFocus)
{
	FInteractPrompt NewPrompt; // default = no target = hide
	if (NewFocus)
	{
		NewPrompt = IInteractable::Execute_GetInteractPrompt(NewFocus, this);
		NewPrompt.Target = NewFocus;
		NewPrompt.Action = (NewPrompt.Slot == EInteractSlot::Primary) ? InteractPrimaryAction : InteractSecondaryAction;
	}

	// Only broadcast on an actual change (focus swapped, or the verb/slot changed)
	if (NewPrompt == CurrentPrompt)
	{
		return;
	}

	FocusedInteractable = NewFocus;
	CurrentPrompt = NewPrompt;
	OnInteractPromptChanged.Broadcast(CurrentPrompt);
}

void ABasePlayer::TryInteract(EInteractSlot Slot)
{
	AActor* Focus = FocusedInteractable.Get();
	if (bIsDead || !IsValid(Focus) || CurrentPrompt.Slot != Slot)
	{
		return;
	}

	if (!IInteractable::Execute_CanInteract(Focus, this))
	{
		if (PlayerAudio) { PlayerAudio->PlayInteractDenied(); }
		return;
	}

	IInteractable::Execute_Interact(Focus, this);
	if (PlayerAudio) { PlayerAudio->PlayInteract(); }
	// The interactable may have destroyed itself - refresh right away instead of
	// leaving a stale prompt up until the next timer tick.
	UpdateFocus();
}

void ABasePlayer::ClearInteraction()
{
	GetWorldTimerManager().ClearTimer(FocusTimerHandle);
	InteractCandidates.Reset();
	SetFocusedInteractable(nullptr);
}

#pragma endregion