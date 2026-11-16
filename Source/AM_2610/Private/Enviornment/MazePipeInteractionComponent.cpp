// Fill out your copyright notice in the Description page of Project Settings.

#include "Enviornment/MazePipeInteractionComponent.h"
#include "Enviornment/MazePipePiece.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"

// Route the existing utility actions to pipes and forward every other press.
class FMazePipeInputState
{
public:
    static TSharedPtr<FMazePipeInputState> Install(UEnhancedInputComponent* Input,
        const UInputAction* LeftAction, const UInputAction* RightAction, TFunction<bool(bool)> Handler);

    bool IsBoundTo(const UEnhancedInputComponent* Input) const;
    void Stop();

private:
    void Route(bool bRotateRight, const FInputActionInstance& Instance);

    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TArray<TUniquePtr<FEnhancedInputActionEventBinding>> LeftBindings;
    TArray<TUniquePtr<FEnhancedInputActionEventBinding>> RightBindings;
    TFunction<bool(bool)> PipeHandler;
    uint32 LeftHandle = 0;
    uint32 RightHandle = 0;
};

TSharedPtr<FMazePipeInputState> FMazePipeInputState::Install(UEnhancedInputComponent* Input,
    const UInputAction* LeftAction, const UInputAction* RightAction, TFunction<bool(bool)> Handler)
{
    if (!IsValid(Input) || !IsValid(LeftAction) || !IsValid(RightAction) || LeftAction == RightAction || !Handler)
    {
        return nullptr;
    }
    TSharedPtr<FMazePipeInputState> Routing = MakeShared<FMazePipeInputState>();
    TArray<uint32> Handles;
    for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input->GetActionEventBindings())
    {
        if (Binding->GetTriggerEvent() != ETriggerEvent::Started)
        {
            continue;
        }
        if (Binding->GetAction() == LeftAction)
        {
            Routing->LeftBindings.Add(Binding->Clone());
            Handles.Add(Binding->GetHandle());
        }
        else if (Binding->GetAction() == RightAction)
        {
            Routing->RightBindings.Add(Binding->Clone());
            Handles.Add(Binding->GetHandle());
        }
    }
    // Wait for both original actions before changing any input bindings.
    if (Routing->LeftBindings.IsEmpty() || Routing->RightBindings.IsEmpty())
    {
        return nullptr;
    }
    Routing->BoundInput = Input;
    Routing->PipeHandler = MoveTemp(Handler);
    for (uint32 Handle : Handles)
    {
        Input->RemoveBindingByHandle(Handle);
    }
    Routing->LeftHandle = Input->BindActionInstanceLambda(LeftAction, ETriggerEvent::Started,
        [Routing](const FInputActionInstance& Instance)
        {
            const TSharedPtr<FMazePipeInputState> KeepAlive = Routing;
            KeepAlive->Route(false, Instance);
        }).GetHandle();
    Routing->RightHandle = Input->BindActionInstanceLambda(RightAction, ETriggerEvent::Started,
        [Routing](const FInputActionInstance& Instance)
        {
            const TSharedPtr<FMazePipeInputState> KeepAlive = Routing;
            KeepAlive->Route(true, Instance);
        }).GetHandle();
    return Routing;
}

bool FMazePipeInputState::IsBoundTo(const UEnhancedInputComponent* Input) const
{
    if (!IsValid(Input) || BoundInput.Get() != Input)
    {
        return false;
    }
    bool bLeft = false;
    bool bRight = false;
    for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input->GetActionEventBindings())
    {
        bLeft |= Binding->GetHandle() == LeftHandle;
        bRight |= Binding->GetHandle() == RightHandle;
    }
    return bLeft && bRight;
}

void FMazePipeInputState::Stop()
{
    // The input component keeps the saved callbacks alive until its own cleanup.
    PipeHandler = TFunction<bool(bool)>();
}

void FMazePipeInputState::Route(bool bRotateRight, const FInputActionInstance& Instance)
{
    const TFunction<bool(bool)> Handler = PipeHandler;
    if (Handler && Handler(bRotateRight))
    {
        return;
    }
    const TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& Bindings = bRotateRight ? RightBindings : LeftBindings;
    for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Bindings)
    {
        Binding->Execute(Instance);
    }
}

UMazePipeInteractionComponent::UMazePipeInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.05f;
    bAutoActivate = true;
}

bool UMazePipeInteractionComponent::PrepareInput(APawn* Player)
{
    UEnhancedInputComponent* Input = Player->FindComponentByClass<UEnhancedInputComponent>();
    if (InputRouting.IsValid() && InputRouting->IsBoundTo(Input)
        && BoundLeftAction.Get() == RotateLeftAction.Get() && BoundRightAction.Get() == RotateRightAction.Get())
    {
        return true;
    }
    ReleaseInput();
    if (!IsValid(Input) || !IsValid(RotateLeftAction) || !IsValid(RotateRightAction))
    {
        return false;
    }
    const TWeakObjectPtr<UMazePipeInteractionComponent> WeakThis(this);
    InputRouting = FMazePipeInputState::Install(Input, RotateLeftAction.Get(), RotateRightAction.Get(),
        [WeakThis](bool bRotateRight)
        {
            return WeakThis.IsValid() && WeakThis->HandlePipeInput(bRotateRight);
        });
    bInputReady = InputRouting.IsValid();
    if (bInputReady)
    {
        BoundLeftAction = RotateLeftAction.Get();
        BoundRightAction = RotateRightAction.Get();
    }
    return bInputReady;
}

AMazePipePiece* UMazePipeInteractionComponent::TracePipe(APawn* Player) const
{
    APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!IsValid(Controller) || !GetWorld() || !FMath::IsFinite(InteractionDistance) || InteractionDistance <= 0.0f)
    {
        return nullptr;
    }
    FVector Origin;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Origin, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MazePipeInteraction), false, Player);
    TArray<AActor*> AttachedActors;
    Player->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin,
        Origin + Rotation.Vector() * FMath::Min(InteractionDistance, 1000.0f), ECC_Visibility, Query))
    {
        return nullptr;
    }
    AMazePipePiece* Pipe = Cast<AMazePipePiece>(Hit.GetActor());
    return IsValid(Pipe) && Pipe->IsRuntimePipe() ? Pipe : nullptr;
}

void UMazePipeInteractionComponent::SetFocusedPipe(AMazePipePiece* Pipe)
{
    const bool bCanRotate = IsValid(Pipe) && Pipe->CanRotatePipe();
    if (FocusedPipe == Pipe && bCanRotateFocusedPipe == bCanRotate)
    {
        return;
    }
    FocusedPipe = Pipe;
    bCanRotateFocusedPipe = bCanRotate;
    OnPipeFocusChanged.Broadcast(Pipe, bCanRotate);
}

FText UMazePipeInteractionComponent::GetRotationPromptVerb() const
{
    const APawn* Player = Cast<APawn>(GetOwner());
    const APlayerController* Controller = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    const ULocalPlayer* LocalPlayer = IsValid(Controller) ? Controller->GetLocalPlayer() : nullptr;
    const UEnhancedInputLocalPlayerSubsystem* Input = IsValid(LocalPlayer)
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    const auto GetKeyText = [Input](const UInputAction* Action, const FKey& Fallback)
        {
            if (IsValid(Input) && IsValid(Action))
            {
                const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Action);
                for (const FKey& Key : Keys)
                {
                    if (Key.IsValid() && !Key.IsGamepadKey())
                    {
                        return Key.GetDisplayName();
                    }
                }
                for (const FKey& Key : Keys)
                {
                    if (Key.IsValid())
                    {
                        return Key.GetDisplayName();
                    }
                }
            }
            return Fallback.GetDisplayName();
        };
    // The player HUD already supplies the first Press [E] to part from the Primary action.
    return FText::Format(NSLOCTEXT("MazePipePuzzle", "RotationVerb", "Rotate Left | Press [{0}] to Rotate Right"),
        GetKeyText(RotateRightAction.Get(), EKeys::F));
}

bool UMazePipeInteractionComponent::HandlePipeInput(bool bRotateRight)
{
    APawn* Player = Cast<APawn>(GetOwner());
    if (!IsActive() || !IsValid(Player) || !Player->IsLocallyControlled())
    {
        return false;
    }
    // Trace again on the press so a recent camera turn cannot rotate the old target.
    AMazePipePiece* Pipe = TracePipe(Player);
    SetFocusedPipe(Pipe);
    if (!IsValid(Pipe))
    {
        return false;
    }
    if (Pipe->CanRotatePipe())
    {
        Pipe->RotatePipe(bRotateRight);
        SetFocusedPipe(Pipe);
    }
    // Locked pipes still own the press so an object behind them is not activated.
    return true;
}

void UMazePipeInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    APawn* Player = Cast<APawn>(GetOwner());
    if (!IsActive() || !IsValid(Player) || !Player->IsLocallyControlled() || !GetWorld() || !GetWorld()->IsGameWorld()
        || GetOwner()->FindComponentByClass<UMazePipeInteractionComponent>() != this)
    {
        ReleaseInput();
        SetFocusedPipe(nullptr);
        return;
    }
    if (!PrepareInput(Player))
    {
        InputWaitSeconds += FMath::Max(0.0f, DeltaTime);
        if (!bWarnedInput && InputWaitSeconds >= 2.0f)
        {
            bWarnedInput = true;
            UE_LOG(LogTemp, Warning, TEXT("Pipe interaction: Assign the player's existing Primary and Secondary Input Actions. Both actions need their existing Started bindings."));
        }
        SetFocusedPipe(nullptr);
        return;
    }
    InputWaitSeconds = 0.0f;
    SetFocusedPipe(TracePipe(Player));
}

void UMazePipeInteractionComponent::ReleaseInput()
{
    if (InputRouting.IsValid())
    {
        InputRouting->Stop();
        InputRouting.Reset();
    }
    bInputReady = false;
    BoundLeftAction.Reset();
    BoundRightAction.Reset();
}

void UMazePipeInteractionComponent::Deactivate()
{
    ReleaseInput();
    SetFocusedPipe(nullptr);
    Super::Deactivate();
}

void UMazePipeInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseInput();
    SetFocusedPipe(nullptr);
    Super::EndPlay(EndPlayReason);
}

void UMazePipeInteractionComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ReleaseInput();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}