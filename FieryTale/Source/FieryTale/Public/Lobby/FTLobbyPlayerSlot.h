// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/FTCharacterTypes.h"
#include "FTLobbyPlayerSlot.generated.h"


class UTextBlock;
class UImage;
class UWidgetSwitcher;

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFTLobbyPlayerSlot : public UUserWidget
{
	GENERATED_BODY()
protected:
	// 위젯 블루프린트 내부의 컴포넌트와 이름이 100% 일치해야 바인딩됩니다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_HeroPortrait;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher_SlotState;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ReadyState;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_TeamColor;

public:
	// 에디터에서 캐릭터 타입별 2D 텍스처(초상화)를 지정할 맵
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TMap<EFTCharacterType, TSoftObjectPtr<UTexture2D>> PortraitMap;

	// 이 슬롯의 데이터(이름, 선택한 캐릭터)를 최신화하는 함수
	void UpdateSlotData(const FString& InPlayerName, EFTCharacterType InCharType,bool bIsReady = false, int32 PlayerIndex = 0);
};
