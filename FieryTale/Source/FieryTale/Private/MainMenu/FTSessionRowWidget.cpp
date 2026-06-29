// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenu/FTSessionRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "MainMenu/FTMainMenuWidget.h"

void UFTSessionRowWidget::NativeConstruct()
{
	Super::NativeConstruct();	
	
	if (Btn_Select)
	{
		Btn_Select->OnClicked.AddDynamic(this, &UFTSessionRowWidget::OnSelectButtonClicked);
	}
}

void UFTSessionRowWidget::OnSelectButtonClicked()
{
	// 버튼을 누르면 부모(메인 메뉴)에게 내 인덱스를 알려줍니다.
	if (MainMenuWidget && SearchIndex != INDEX_NONE)
	{
		MainMenuWidget->SetSelectedSessionIndex(SearchIndex);
	}
}

void UFTSessionRowWidget::SetupRow(UFTMainMenuWidget* InMainMenu, const FFTSessionInfo& SessionInfo)
{
	MainMenuWidget = InMainMenu;
	SearchIndex = SessionInfo.SearchIndex;

	if (Text_RoomName)
	{
		Text_RoomName->SetText(FText::FromString(SessionInfo.RoomName));
	}

	if (Text_PlayerCount)
	{
		FString PCount = FString::Printf(TEXT("%d / %d"), SessionInfo.CurrentPlayers, SessionInfo.MaxPlayers);
		Text_PlayerCount->SetText(FText::FromString(PCount));
	}

	if (Text_Ping)
	{
		FString PingStr = FString::Printf(TEXT("%d ms"), SessionInfo.PingMs);
		Text_Ping->SetText(FText::FromString(PingStr));
	}
}
