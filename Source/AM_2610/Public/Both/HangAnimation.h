// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HangAnimation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFalling);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFlying);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHangAround);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerCaught);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewLocation);
/**
 * 
 */
UCLASS()
class AM_2610_API UHangAnimation : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetFalling(bool bIsFalling) { IsFalling = bIsFalling; }	
	void SetFlying(bool bIsFlying) { IsFlying = bIsFlying; }	
	void SetAttacking(bool bIsAttacking) { IsAttacking = bIsAttacking; }
	bool GetFalling() const { return IsFalling; }
	bool GetFlying() const { return IsFlying; }
	bool GetAttacking() const { return IsAttacking; }

	bool IsMoving() const { return Speed > 0.0f; }

	UFUNCTION()
	void AnimFall();

	UFUNCTION()
	void AnimFly();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* FallAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* JumpAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Default)
	UAnimSequenceBase* AttackAsset;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnFalling OnFalling;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnFlying OnFlying;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnHangAround OnHangAround;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnPlayerCaught OnPlayerCaught;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Movement Events")
	FOnNewLocation OnNewLocation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Default)
	FName ActionSlotName;

	virtual void NativeInitializeAnimation() override;

protected:
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Speed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Variables|Default")
	float Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Default", meta = (AllowPrivateAccess = "true"))
	bool IsFalling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variables|Default", meta = (AllowPrivateAccess = "true"))
	bool IsFlying;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variables|Default", meta = (AllowPrivateAccess = "true"))
	bool IsAttacking;

private:
	UFUNCTION()
	void AnimHang();

	UFUNCTION(BlueprintCallable)
	void LeaveGround();

	UFUNCTION(BlueprintCallable)
	void LeaveCeiling();

	UFUNCTION(BlueprintCallable)
	void AnimAttack();
};
