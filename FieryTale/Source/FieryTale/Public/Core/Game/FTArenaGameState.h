// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Character/FTCharacterTypes.h"
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

	void AddTeamScore(EFTTeam ScoringTeam);
	
	// UI에서 팀 스코어를 읽어갈 수 있도록 제공하는 함수
	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetBlueTeamKills() const { return BlueTeamKills; }

	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetRedTeamKills() const { return RedTeamKills; }
	

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "Game State")
	EFTArenaMatchState CurrentMatchState;

	UFUNCTION()
	void OnRep_CurrentMatchState();
	
	UPROPERTY(Replicated)
	int32 BlueTeamKills = 0;

	UPROPERTY(Replicated)
	int32 RedTeamKills = 0;
	
};
