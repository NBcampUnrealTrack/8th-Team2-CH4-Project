// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Scoreboard/FTScoreboardRowWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UFTScoreboardRowWidget::SetupRow(const FText& PlayerName, const FText& CharacterName, UTexture2D* Portrait, int32 Kills, int32 Deaths)
{
	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(PlayerName);
	}
	if (Text_CharacterName)
	{
		Text_CharacterName->SetText(CharacterName);
	}
	//	포트레이트가 아직 로드 안 됐거나 없는 캐릭터면 브러시를 건드리지 않는다(디자이너 기본 이미지 유지).
	if (Img_Portrait && Portrait)
	{
		Img_Portrait->SetBrushFromTexture(Portrait);
	}

	UpdateScore(Kills, Deaths);
}

void UFTScoreboardRowWidget::UpdateScore(int32 Kills, int32 Deaths)
{
	if (Text_Kills)
	{
		Text_Kills->SetText(FText::AsNumber(Kills));
	}
	if (Text_Deaths)
	{
		Text_Deaths->SetText(FText::AsNumber(Deaths));
	}
}
