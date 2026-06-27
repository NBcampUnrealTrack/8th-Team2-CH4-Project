// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerState.h"

#include "FieryTaleLog.h"
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
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 3) 서버 SetReady: %s, %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"),
		bInReady ? TEXT("Ready") : TEXT("NotReady"));

	if (bIsReady != bInReady)
	{
		bIsReady = bInReady;
		// Listen 서버(호스트)의 로컬 UI는 OnRep 이 호출되지 않으므로 여기서 직접 알린다.
		OnReadyStateChanged.Broadcast();
	}
}

void AFTLobbyPlayerState::OnRep_IsReady()
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 4) 복제 수신(클라) OnRep_IsReady: %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"));
	OnReadyStateChanged.Broadcast();
}
