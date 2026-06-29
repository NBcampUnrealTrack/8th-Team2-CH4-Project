// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTLobbyWidget.generated.h"

/**
 * 
 */

class UButton;
class UTextBlock;
class AFTLobbyPlayerController;
class AFTLobbyPlayerState;

UCLASS()
class FIERYTALE_API UFTLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	// 바인딩을 위한 버튼
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Ready;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_StartGame;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_ReadyStatus;
	
public:
	void InitWidget(AFTLobbyPlayerController* InPC, AFTLobbyPlayerState* InPS);

private:
	
	// 바인딩 함수
	UFUNCTION()
	void OnReadyClicked();

	UFUNCTION()
	void OnStartGameClicked();
	
	UFUNCTION()
	void OnReadyStateChanged();

	// 플레이어 컨트톨러, 플레이어 스테이트 캐싱용 포인터
	UPROPERTY()
	AFTLobbyPlayerController* LobbyPC;

	UPROPERTY()
	AFTLobbyPlayerState* LobbyPS;
	
};
