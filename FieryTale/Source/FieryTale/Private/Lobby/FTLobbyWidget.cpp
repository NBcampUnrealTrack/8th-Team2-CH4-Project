// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Character/FTPlayerController.h"
#include "Lobby/FTLobbyPlayerState.h"

void UFTLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// 버튼 이벤트 바인딩
	if (Btn_Ready)
	{
		Btn_Ready->OnClicked.AddDynamic(this, &UFTLobbyWidget::OnReadyClicked);
	}
	if (Btn_StartGame)
	{
		Btn_StartGame->OnClicked.AddDynamic(this, &UFTLobbyWidget::OnStartGameClicked);
	}
}

void UFTLobbyWidget::InitWidget(AFTPlayerController* InPC, AFTLobbyPlayerState* InPS)
{
	LobbyPC = InPC;
	LobbyPS = InPS;
	
	if (LobbyPS)
	{
		// 안전하게 바인딩
		LobbyPS->OnReadyStateChanged.RemoveDynamic(this, &UFTLobbyWidget::OnReadyStateChanged); // 중복 방지
		LobbyPS->OnReadyStateChanged.AddDynamic(this, &UFTLobbyWidget::OnReadyStateChanged);
		
		// 현재 상태 반영
		OnReadyStateChanged();
	}
}

void UFTLobbyWidget::OnReadyClicked()
{
	if (LobbyPC && !LobbyPS)
	{
		LobbyPS = LobbyPC->GetPlayerState<AFTLobbyPlayerState>();
		if (LobbyPS)
		{
			LobbyPS->OnReadyStateChanged.AddDynamic(this, &UFTLobbyWidget::OnReadyStateChanged);
		}
	}

	if (LobbyPC && LobbyPS)
	{
		bool bNewReadyState = !LobbyPS->IsReady();
		LobbyPC->RequestSetReady(bNewReadyState);
	}
}

void UFTLobbyWidget::OnStartGameClicked()
{
	if (LobbyPC)
	{
		// 강제 게임 시작 요청
		LobbyPC->RequestStartMatch();
	}
}

void UFTLobbyWidget::OnReadyStateChanged()
{
	// bIsReady 값이 리플리케이트(OnRep) 되거나 로컬에서 변경될 때마다 호출됨
	if (LobbyPS && Text_ReadyStatus)
	{
		if (LobbyPS->IsReady())
		{
			Text_ReadyStatus->SetText(FText::FromString(TEXT("상태: 준비 완료!")));
			Text_ReadyStatus->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
		}
		else
		{
			Text_ReadyStatus->SetText(FText::FromString(TEXT("상태: 대기 중...")));
			Text_ReadyStatus->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}