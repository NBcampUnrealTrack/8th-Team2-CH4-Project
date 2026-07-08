// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FTResultWidget.h"
#include "Components/TextBlock.h"

void UFTResultWidget::SetMatchResult(bool bIsVictory)
{
	if (!Text_MatchResult) return;

	if (bIsVictory)
	{
		Text_MatchResult->SetText(FText::FromString(TEXT("승 리 !")));
		// 승리 시 텍스트 색상을 파란색/노란색 계열로 변경 (오버워치 느낌)
		Text_MatchResult->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.5f, 1.0f))); 
	}
	else
	{
		Text_MatchResult->SetText(FText::FromString(TEXT("패 배 ...")));
		// 패배 시 텍스트 색상을 빨간색으로 변경
		Text_MatchResult->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.1f, 0.1f)));
	}
}