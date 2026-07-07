// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FTLobbyPlayerSlot.h"
#include "Blueprint/UserWidget.h"
#include "Character/FTPlayerController.h"
#include "Character/FTCharacterTypes.h"
#include "FTLobbyWidget.generated.h"

/**
 * 
 */

class UButton;
class UTextBlock;
class AFTPlayerController;
class AFTLobbyPlayerState;
class UHorizontalBox;
class AGameStateBase;

UCLASS()
class FIERYTALE_API UFTLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 바인딩을 위한 버튼
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Ready;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_StartGame;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_ReadyStatus;
	
	// 캐릭터 선택 슬롯
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectRedHood;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectAladdin;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectKaguya;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectAlice;
	
	// 팀원들의 초상화 위젯들이 담길 가로형 박스
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* TeamRosterBox;

	// 팀원 1명을 표시할 서브 위젯 클래스 (에디터에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UFTLobbyPlayerSlot> PlayerSlotWidgetClass;
	
public:
	void InitWidget(AFTPlayerController* InPC, AFTLobbyPlayerState* InPS);
	
	//  팀원 목록을 싹 지우고 최신화하는 함수
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshTeamRoster();

private:
	
	// 바인딩 함수
	UFUNCTION()
	void OnReadyClicked();
	UFUNCTION()
	void OnStartGameClicked();
	UFUNCTION()
	void OnReadyStateChanged();
	
	UFUNCTION()
	void OnRedHoodClicked();
	UFUNCTION()
	void OnAladdinClicked();
	UFUNCTION()
	void OnKaguyaClicked();
	UFUNCTION()
	void OnAliceClicked();
	
	// 주기적으로 타이머 호출해서 인원수 체크
	FTimerHandle RosterCheckTimerHandle;
	int32 CachedPlayerCount = 0;
	
	UFUNCTION()
	void CheckRosterCount();

	// 플레이어 컨트톨러, 플레이어 스테이트 캐싱용 포인터
	UPROPERTY()
	AFTPlayerController* LobbyPC;

	UPROPERTY()
	AFTLobbyPlayerState* LobbyPS;
	
};
