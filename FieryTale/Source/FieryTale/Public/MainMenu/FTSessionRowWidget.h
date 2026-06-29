// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/FTSessionSubsystem.h"
#include "FTSessionRowWidget.generated.h"

/**
 * 
 */

class UTextBlock;
class UButton;
class UFTMainMenuWidget;

UCLASS()
class FIERYTALE_API UFTSessionRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_RoomName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_PlayerCount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Ping;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Select;
	
private:
	UFUNCTION()
	void OnSelectButtonClicked();

	UPROPERTY()
	UFTMainMenuWidget* MainMenuWidget;

	int32 SearchIndex = INDEX_NONE;

public:
	// 메인 메뉴에서 이 함수를 호출하여 검색된 방 데이터를 스크롤에 보여주는 구조
	void SetupRow(UFTMainMenuWidget* InMainMenu, const FFTSessionInfo& SessionInfo);
};
