// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerController.h"

#include "FieryTaleLog.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyGameMode.h"
#include "Engine/World.h"

void AFTLobbyPlayerController::RequestSetReady(bool bReady)
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 1) 클라 요청 RequestSetReady(%s) → ServerRPC (%s)"),
		bReady ? TEXT("true") : TEXT("false"), *GetName());
	ServerSetReady(bReady);
}

void AFTLobbyPlayerController::RequestStartMatch()
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 호스트 시작 버튼 RequestStartMatch → ServerRPC (%s)"), *GetName());
	ServerStartMatch();
}

void AFTLobbyPlayerController::ServerSetReady_Implementation(bool bReady)
{
	AFTLobbyPlayerState* LobbyPlayerState = GetPlayerState<AFTLobbyPlayerState>();
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 2) 서버 수신(권한=%s): bReady=%s, PlayerState 캐스트=%s"),
		HasAuthority() ? TEXT("Y") : TEXT("N"), bReady ? TEXT("true") : TEXT("false"),
		LobbyPlayerState ? TEXT("OK") : TEXT("NULL(AFTLobbyPlayerState 아님 - GameMode 의심)"));

	if (LobbyPlayerState)
	{
		LobbyPlayerState->SetReady(bReady);
	}

	if (UWorld* World = GetWorld())
	{
		if (AFTLobbyGameMode* GameMode = World->GetAuthGameMode<AFTLobbyGameMode>())
		{
			GameMode->NotifyReadyStateChanged();
		}
		else
		{
			UE_LOG(LogFTSession, Warning,
				TEXT("[Ready] 2-주의) AuthGameMode 가 AFTLobbyGameMode 아님 - 맵 GameMode 미설정 가능"));
		}
	}
}

void AFTLobbyPlayerController::ServerStartMatch_Implementation()
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 매치 시작 RPC 서버 수신 (권한=%s)"), HasAuthority() ? TEXT("Y") : TEXT("N"));
	if (UWorld* World = GetWorld())
	{
		if (AFTLobbyGameMode* GameMode = World->GetAuthGameMode<AFTLobbyGameMode>())
		{
			GameMode->RequestStartMatch(this);
		}
		else
		{
			UE_LOG(LogFTSession, Warning, TEXT("[Ready] AuthGameMode 가 AFTLobbyGameMode 아님"));
		}
	}
}
