// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTScoreWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class FIERYTALE_API UFTScoreWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- 팀 스코어 (상단 중앙 배치 추천) ---
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_BlueScore;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_RedScore;

	// --- 개인 스코어 (우측 상단 또는 하단 배치 추천) ---
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_MyKills;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_MyDeaths;
};
