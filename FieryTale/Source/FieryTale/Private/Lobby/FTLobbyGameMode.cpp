// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyGameMode.h"

#include "FieryTaleLog.h"
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

	UE_LOG(LogFTSession, Log, TEXT("[Lobby] PostLogin: %s 접속 → 현재 인원 %d"),
		NewPlayer ? *NewPlayer->GetName() : TEXT("NULL"),
		GameState ? GameState->PlayerArray.Num() : -1);

	NotifyReadyStateChanged();
}

void AFTLobbyGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogFTSession, Log, TEXT("[Lobby] Logout: %s 이탈 → 남은 인원 %d"),
		Exiting ? *Exiting->GetName() : TEXT("NULL"),
		GameState ? FMath::Max(0, GameState->PlayerArray.Num() - 1) : -1);

	Super::Logout(Exiting);

	NotifyReadyStateChanged();
}

void AFTLobbyGameMode::NotifyReadyStateChanged()
{
	const int32 NumPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	const bool bAllReady = AreAllPlayersReady();
	UE_LOG(LogFTSession, Log, TEXT("[Lobby] Ready 재평가: 인원=%d, 최소=%d, 전원준비=%s, 자동시작=%s"),
		NumPlayers, MinPlayersToStart, bAllReady ? TEXT("Y") : TEXT("N"),
		bAutoStartWhenAllReady ? TEXT("Y") : TEXT("N"));

	if (bAutoStartWhenAllReady && bAllReady)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Lobby] 조건 충족 → 매치 시작"));
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
	const int32 NumPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	UE_LOG(LogFTSession, Log, TEXT("[Lobby] 호스트 매치 시작 요청: %s, 인원=%d/%d"),
		Requester ? *Requester->GetName() : TEXT("NULL"), NumPlayers, MinPlayersToStart);

	// 프로토타입: 최소 인원만 충족하면 시작한다. (추후 호스트 전용 제한으로 강화 가능)
	if (GameState && GameState->PlayerArray.Num() >= MinPlayersToStart)
	{
		TravelToMatch();
	}
	else
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Lobby] 인원 부족으로 시작 거부"));
	}
}

void AFTLobbyGameMode::TravelToMatch()
{
	if (bMatchStarting)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Lobby] TravelToMatch 중복 호출 무시 (이미 시작 중)"));
		return;
	}
	bMatchStarting = true;

	if (UWorld* World = GetWorld())
	{
		const FString TravelUrl = GameMapPath + TEXT("?listen");
		UE_LOG(LogFTSession, Log, TEXT("[Lobby] 매치 맵 ServerTravel → %s"), *TravelUrl);
		World->ServerTravel(TravelUrl);
	}
	else
	{
		UE_LOG(LogFTSession, Error, TEXT("[Lobby] World 없음 - ServerTravel 불가"));
	}
}
