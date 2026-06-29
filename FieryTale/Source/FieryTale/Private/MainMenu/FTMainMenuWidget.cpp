// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenu/FTMainMenuWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "MainMenu/FTSessionRowWidget.h"
#include "Engine/GameInstance.h"


void UFTMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		SessionSubsystem = GameInstance->GetSubsystem<UFTSessionSubsystem>();
		if (SessionSubsystem)
		{
			// 서브시스템의 완료 이벤트에 UI 함수 바인딩
			SessionSubsystem->OnCreateSessionCompleteEvent.AddDynamic(this, &UFTMainMenuWidget::HandleCreateSessionComplete);
			SessionSubsystem->OnFindSessionsCompleteEvent.AddDynamic(this, &UFTMainMenuWidget::HandleFindSessionsComplete);
			SessionSubsystem->OnJoinSessionCompleteEvent.AddDynamic(this, &UFTMainMenuWidget::HandleJoinSessionComplete);
		}
	}

	if (Btn_HostSession)  Btn_HostSession->OnClicked.AddDynamic(this, &UFTMainMenuWidget::OnHostButtonClicked);
	if (Btn_FindSessions) Btn_FindSessions->OnClicked.AddDynamic(this, &UFTMainMenuWidget::OnFindButtonClicked);
	if (Btn_JoinSession)  Btn_JoinSession->OnClicked.AddDynamic(this, &UFTMainMenuWidget::OnJoinButtonClicked);
}

void UFTMainMenuWidget::OnHostButtonClicked()
{
	if (!SessionSubsystem) return;
	
	if (Btn_HostSession) Btn_HostSession->SetIsEnabled(false);
	if (Btn_FindSessions) Btn_FindSessions->SetIsEnabled(false);

	FString RoomName = Input_RoomName ? Input_RoomName->GetText().ToString() : TEXT("FieryTale Room");
	if (RoomName.IsEmpty()) RoomName = TEXT("FieryTale Room");

	int32 MaxPlayers = 4;
	if (Input_MaxPlayers)
	{
		MaxPlayers = FCString::Atoi(*Input_MaxPlayers->GetText().ToString());
		MaxPlayers = FMath::Clamp(MaxPlayers, 2, 8); // 최소 2, 최대 8명으로 강제
	}

	if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("방 생성 중...")));
	
	// FTSessionSubsystem의 HostSession 호출 (비밀번호 없음, LAN 사용 안함 가정)
	SessionSubsystem->HostSession(MaxPlayers, RoomName, TEXT(""), true);
}

void UFTMainMenuWidget::OnFindButtonClicked()
{
	if (!SessionSubsystem) return;

	if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("방 검색 중...")));
	if (Scroll_SessionList) Scroll_SessionList->ClearChildren();
	
	SelectedSessionIndex = INDEX_NONE;
	SessionSubsystem->FindSessions(20, true);
}

void UFTMainMenuWidget::OnJoinButtonClicked()
{
	if (!SessionSubsystem) return;

	if (SelectedSessionIndex == INDEX_NONE)
	{
		if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("리스트에서 참여할 방을 먼저 선택하세요.")));
		return;
	}

	if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("서버 접속 시도 중...")));
	SessionSubsystem->JoinSessionByIndex(SelectedSessionIndex, TEXT(""));
}

void UFTMainMenuWidget::HandleCreateSessionComplete(bool bWasSuccessful)
{
	if (Text_StatusMessage)
	{
		Text_StatusMessage->SetText(bWasSuccessful ? FText::FromString(TEXT("방 생성 성공! 로비로 이동합니다.")) : FText::FromString(TEXT("방 생성 실패.")));
	}
}

void UFTMainMenuWidget::HandleFindSessionsComplete(const TArray<FFTSessionInfo>& SessionResults, bool bWasSuccessful)
{
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("검색된 방이 없습니다.")));
		return;
	}

	if (Text_StatusMessage) Text_StatusMessage->SetText(FText::FromString(TEXT("검색 완료. 방을 선택하세요.")));
	if (!Scroll_SessionList || !SessionRowClass) return;

	for (const FFTSessionInfo& Info : SessionResults)
	{
		UFTSessionRowWidget* RowWidget = CreateWidget<UFTSessionRowWidget>(this, SessionRowClass);
		if (RowWidget)
		{
			RowWidget->SetupRow(this, Info);
			Scroll_SessionList->AddChild(RowWidget);
		}
	}
}

void UFTMainMenuWidget::HandleJoinSessionComplete(bool bWasSuccessful)
{
	if (Text_StatusMessage)
	{
		Text_StatusMessage->SetText(bWasSuccessful ? FText::FromString(TEXT("접속 성공! 맵을 로드합니다.")) : FText::FromString(TEXT("접속 실패.")));
	}
}

void UFTMainMenuWidget::SetSelectedSessionIndex(int32 Index)
{
	SelectedSessionIndex = Index;
	if (Text_StatusMessage)
	{
		Text_StatusMessage->SetText(FText::FromString(TEXT("방 선택됨. '참여' 버튼을 누르세요.")));
	}
}
