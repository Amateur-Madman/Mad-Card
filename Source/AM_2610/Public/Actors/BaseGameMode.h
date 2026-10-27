// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BaseGameMode.generated.h"

/**
 * 
 */
UCLASS()
class AM_2610_API ABaseGameMode : public AGameModeBase
{
	GENERATED_BODY()
public: 

	// Single entry points for ending the level - called by pickups, the player's death, etc.
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void TriggerWin();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void TriggerLose();

	bool IsGameOver() const { return bGameOver; }
//----DEPRECATED:--------
// Registers NewEnemy with the win condition: counts it toward NumberOfEnemies and binds
// its OnDestroyed to RemoveEnemy. Used by BeginPlay's initial sweep for actors already in
// the level, and called directly by ASpawner - for itself, and for every agent it spawns
// after BeginPlay has already run and the initial sweep is long done.
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void RegisterEnemy(AActor* NewEnemy);

protected:
	virtual void BeginPlay() override;

	// The single BasePlayer found in the level - set once during the BeginPlay sweep
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	class ABasePlayer* CurrentPlayer;

	// Count of BaseCharacters in the level that aren't the player - every failed
	// Cast<ABasePlayer> during the sweep increments this once. Still writable from
	// Blueprint since RemoveEnemy() (not yet ported) decrements it directly.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GameFlow")
	int32 NumberOfEnemies = 0;

	// Soft reference - only resolved/loaded when actually needed, not at construction time
	UPROPERTY(EditDefaultsOnly, Category = "GameFlow")
	TSoftClassPtr<class UResultsWidget> ResultsWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameFlow")
	class UResultsWidget* ResultsWidgetObject;

	// Bound to each enemy's OnDestroyed (signature matches AActor::OnDestroyed exactly) -
	// decrements NumberOfEnemies and announces a win once none are left
	UFUNCTION()
	void RemoveEnemy(AActor* DestroyedActor);

	// Bound to CurrentPlayer's OnPlayerLost - announces a loss
	UFUNCTION()
	void RemovePlayer();

private:
	// Adds the results widget and switches to UI-only input
	void ShowResults();

	// Guards against double triggers (e.g. pickup + death animation finishing)
	bool bGameOver = false;
};
