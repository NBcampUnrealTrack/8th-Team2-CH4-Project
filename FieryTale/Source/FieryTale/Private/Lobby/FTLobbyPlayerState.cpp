// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

AFTLobbyPlayerState::AFTLobbyPlayerState()
{
}

void AFTLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTLobbyPlayerState, bIsReady);
}

void AFTLobbyPlayerState::SetReady(bool bInReady)
{
	if (bIsReady != bInReady)
	{
		bIsReady = bInReady;
		// Listen 서버(호스트)의 로컬 UI는 OnRep 이 호출되지 않으므로 여기서 직접 알린다.
		OnReadyStateChanged.Broadcast();
	}
}

void AFTLobbyPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast();
}
