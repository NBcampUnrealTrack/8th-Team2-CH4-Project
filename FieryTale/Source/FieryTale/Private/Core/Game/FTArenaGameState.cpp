// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/FTArenaGameState.h"
#include "FieryTaleLog.h"
#include "Net/UnrealNetwork.h"

AFTArenaGameState::AFTArenaGameState()
{
	CurrentMatchState = EFTArenaMatchState::WaitingToStart;
}

void AFTArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTArenaGameState, CurrentMatchState);
}

void AFTArenaGameState::SetArenaMatchState(EFTArenaMatchState NewState)
{
	if (HasAuthority() && CurrentMatchState != NewState)
	{
		CurrentMatchState = NewState;
		OnRep_CurrentMatchState();
	}
}

void AFTArenaGameState::OnRep_CurrentMatchState()
{
	UE_LOG(LogFTSession, Log, TEXT("[Arena] 전장 매치 상태 변경됨: %d"), (int32)CurrentMatchState);
}