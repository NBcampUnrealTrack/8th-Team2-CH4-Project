// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyGameMode.h"

#include "FieryTaleLog.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h" // NAME_GameSession 사용을 위함

AFTLobbyGameMode::AFTLobbyGameMode()
{
	bUseSeamlessTravel = true;

	// 코드 기본값. 에디터에서 BP GameMode 로 덮어써도 된다.
	PlayerStateClass = AFTLobbyPlayerState::StaticClass();
	PlayerControllerClass = AFTLobbyPlayerController::StaticClass();
}

void AFTLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (OSS)
	{
		IOnlineSessionPtr SessionInterface = OSS->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
			if (Session)
			{
				// 세션에 저장된 정원(MaxPlayers)을 시작 필수 인원(MinPlayersToStart)으로 덮어씌웁니다.
				MinPlayersToStart = Session->SessionSettings.NumPublicConnections;
				UE_LOG(LogFTSession, Log, TEXT("[Lobby] 메인 메뉴 연동 완료: 게임 시작 필수 인원이 %d명으로 고정되었습니다."), MinPlayersToStart);
			}
		}
	}
}

void AFTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (AFTLobbyPlayerState* PS = NewPlayer->GetPlayerState<AFTLobbyPlayerState>())
	{
		// GameState의 PlayerArray 크기 - 1 을 하면 0번부터 순서대로 배정됩니다.
		if (GameState)
		{
			PS->PlayerIndex = GameState->PlayerArray.Num() - 1;
			UE_LOG(LogFTSession, Log, TEXT("[Lobby] %s 님에게 %d번 진열대 자리가 배정되었습니다."), *NewPlayer->GetName(), PS->PlayerIndex);
		}
	}

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

	return NumLobbyPlayers > 0 && NumLobbyPlayers >= MinPlayersToStart;
}

void AFTLobbyGameMode::RequestStartMatch(APlayerController* Requester)
{
	const int32 NumPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	UE_LOG(LogFTSession, Log, TEXT("[Lobby] 호스트 매치 시작 요청: %s, 인원=%d/%d"),
		Requester ? *Requester->GetName() : TEXT("NULL"), NumPlayers, MinPlayersToStart);

	// 1. 먼저 인원이 다 찼는지 검사
	if (NumPlayers >= MinPlayersToStart)
	{
		// 2. 꽉 찬 인원이 모두 레디를 눌렀는지 검사
		if (AreAllPlayersReady())
		{
			UE_LOG(LogFTSession, Log, TEXT("[Lobby] 전원 레디 확인 완료! 투기장으로 이동합니다."));
			TravelToMatch();
		}
		else
		{
			UE_LOG(LogFTSession, Warning, TEXT("[Lobby] 아직 레디하지 않은 플레이어가 있어 시작할 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Lobby] 아직 %d명이 다 모이지 않아 시작할 수 없습니다."), MinPlayersToStart);
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
