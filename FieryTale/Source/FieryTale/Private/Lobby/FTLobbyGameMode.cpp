// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyGameMode.h"

#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

AFTLobbyGameMode::AFTLobbyGameMode()
{
	bUseSeamlessTravel = true;

	// 코드 기본값. 에디터에서 BP GameMode 로 덮어써도 된다.
	PlayerStateClass = AFTLobbyPlayerState::StaticClass();
	PlayerControllerClass = AFTLobbyPlayerController::StaticClass();
}

void AFTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	NotifyReadyStateChanged();
}

void AFTLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	NotifyReadyStateChanged();
}

void AFTLobbyGameMode::NotifyReadyStateChanged()
{
	if (bAutoStartWhenAllReady && AreAllPlayersReady())
	{
		TravelToMatch();
	}
}

bool AFTLobbyGameMode::AreAllPlayersReady() const
{
	if (GameState == nullptr)
	{
		return false;
	}

	int32 NumLobbyPlayers = 0;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AFTLobbyPlayerState* LobbyPlayerState = Cast<AFTLobbyPlayerState>(PlayerState);
		if (LobbyPlayerState == nullptr)
		{
			continue;
		}

		++NumLobbyPlayers;
		if (!LobbyPlayerState->IsReady())
		{
			return false;
		}
	}

	return NumLobbyPlayers >= MinPlayersToStart;
}

void AFTLobbyGameMode::RequestStartMatch(APlayerController* Requester)
{
	// 프로토타입: 최소 인원만 충족하면 시작한다. (추후 호스트 전용 제한으로 강화 가능)
	if (GameState && GameState->PlayerArray.Num() >= MinPlayersToStart)
	{
		TravelToMatch();
	}
}

void AFTLobbyGameMode::TravelToMatch()
{
	if (bMatchStarting)
	{
		return;
	}
	bMatchStarting = true;

	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(GameMapPath + TEXT("?listen"));
	}
}
