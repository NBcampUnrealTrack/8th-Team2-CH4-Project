// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerController.h"

#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyGameMode.h"
#include "Engine/World.h"

void AFTLobbyPlayerController::RequestSetReady(bool bReady)
{
	ServerSetReady(bReady);
}

void AFTLobbyPlayerController::RequestStartMatch()
{
	ServerStartMatch();
}

void AFTLobbyPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (AFTLobbyPlayerState* LobbyPlayerState = GetPlayerState<AFTLobbyPlayerState>())
	{
		LobbyPlayerState->SetReady(bReady);
	}

	if (UWorld* World = GetWorld())
	{
		if (AFTLobbyGameMode* GameMode = World->GetAuthGameMode<AFTLobbyGameMode>())
		{
			GameMode->NotifyReadyStateChanged();
		}
	}
}

void AFTLobbyPlayerController::ServerStartMatch_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTLobbyGameMode* GameMode = World->GetAuthGameMode<AFTLobbyGameMode>())
		{
			GameMode->RequestStartMatch(this);
		}
	}
}
