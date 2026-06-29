// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerController.h"

#include "FieryTaleLog.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyGameMode.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Lobby/FTLobbyWidget.h"

void AFTLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 화면에만 UI를 띄움
	if (IsLocalPlayerController() && LobbyWidgetClass)
	{
		// UUserWidget 대신 UFTLobbyWidget으로 캐스팅하여 생성
		UFTLobbyWidget* LobbyUI = CreateWidget<UFTLobbyWidget>(this, LobbyWidgetClass);
		if (LobbyUI)
		{
			LobbyWidgetInstance = LobbyUI;
			LobbyWidgetInstance->AddToViewport();
			
			bShowMouseCursor = true;
			FInputModeUIOnly InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);

			// 플레이어 스테이트가 존재한다면 바로 초기화
			if (AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>())
			{
				LobbyUI->InitWidget(this, LobbyPS);
			}
		}
	}
}

void AFTLobbyPlayerController::OnRep_PlayerState() // 플레이어 스테이트 복제완료시 자동 호출되는 함수
{
	Super::OnRep_PlayerState();
	
	// 클라이언트 컴퓨터에서 뒤늦게 PlayerState가 전달되었을 때 UI와 연결
	UFTLobbyWidget* LobbyUI = Cast<UFTLobbyWidget>(LobbyWidgetInstance);
	AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>();
	
	if (LobbyUI && LobbyPS)
	{
		LobbyUI->InitWidget(this, LobbyPS);
	}
}

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
