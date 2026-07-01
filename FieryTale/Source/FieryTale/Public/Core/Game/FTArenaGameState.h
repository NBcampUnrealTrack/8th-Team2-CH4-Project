// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "FTArenaGameState.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EFTArenaMatchState : uint8
{
	WaitingToStart,
	InProgress,
	GameOver
};

UCLASS()
class FIERYTALE_API AFTArenaGameState : public AGameState
{
	GENERATED_BODY()

public:
	AFTArenaGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetArenaMatchState(EFTArenaMatchState NewState);
	EFTArenaMatchState GetArenaMatchState() const { return CurrentMatchState; }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "Game State")
	EFTArenaMatchState CurrentMatchState;

	UFUNCTION()
	void OnRep_CurrentMatchState();
	
};
