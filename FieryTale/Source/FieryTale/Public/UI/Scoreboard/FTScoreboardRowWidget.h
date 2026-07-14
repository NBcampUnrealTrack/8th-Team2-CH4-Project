// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTScoreboardRowWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/**
 *	스코어보드에서 플레이어 1명을 표시하는 행(Row) 베이스 클래스.
 *	보드(UFTScoreboardWidget)가 플레이어별로 하나씩 생성해 팀 VerticalBox에 넣는다.
 *	이 클래스는 "표시"만 담당한다 — 데이터 조회/복제는 보드와 PlayerState 몫이다.
 *
 *	BP에서 이 클래스를 상속해 레이아웃만 구성하고, 아래 BindWidget 요소를 같은 이름으로 배치한다:
 *	  - Img_Portrait      (UImage)     : 캐릭터 초상화
 *	  - Text_PlayerName   (UTextBlock) : 플레이어(계정) 이름
 *	  - Text_CharacterName(UTextBlock) : 선택 캐릭터(영웅) 이름
 *	  - Text_Kills        (UTextBlock) : 킬 수
 *	  - Text_Deaths       (UTextBlock) : 데스 수
 */
UCLASS()
class FIERYTALE_API UFTScoreboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//	플레이어 1명의 정적 정보를 한 번에 채운다. 포트레이트는 보드가 이미 로드해 넘겨준다(소프트 참조 해제는 보드에서).
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Scoreboard")
	void SetupRow(const FText& PlayerName, const FText& CharacterName, UTexture2D* Portrait, int32 Kills, int32 Deaths);

	//	매 프레임 바뀔 수 있는 K/D만 갱신한다 (행 재생성 없이 숫자만 업데이트).
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Scoreboard")
	void UpdateScore(int32 Kills, int32 Deaths);

protected:
	UPROPERTY(meta = (BindWidget))
	UImage* Img_Portrait;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_PlayerName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_CharacterName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Kills;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Deaths;
};
