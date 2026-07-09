// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FTScoreWidget.h"
#include "Components/TextBlock.h"
#include "Core/Game/FTArenaGameState.h"
#include "Character/FTPlayerState.h"

void UFTScoreWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 1. 팀 스코어 업데이트 (GameState에서 가져옴)
	if (AFTArenaGameState* GS = GetWorld()->GetGameState<AFTArenaGameState>())
	{
		if (Text_BlueScore)
		{
			Text_BlueScore->SetText(FText::AsNumber(GS->GetBlueTeamKills()));
		}
		if (Text_RedScore)
		{
			Text_RedScore->SetText(FText::AsNumber(GS->GetRedTeamKills()));
		}
	}

	// 2. 개인 킬/데스 업데이트 (내 PlayerState에서 가져옴)
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AFTPlayerState* PS = PC->GetPlayerState<AFTPlayerState>())
		{
			if (Text_MyKills)
			{
				Text_MyKills->SetText(FText::Format(FText::FromString(TEXT("K: {0}")), FText::AsNumber(PS->GetKills())));
			}
			if (Text_MyDeaths)
			{
				Text_MyDeaths->SetText(FText::Format(FText::FromString(TEXT("D: {0}")), FText::AsNumber(PS->GetDeaths())));
			}
		}
	}
}