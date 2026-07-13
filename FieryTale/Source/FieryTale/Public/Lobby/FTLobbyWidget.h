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
	UButton* Btn_ReturnToMain;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_ReadyStatus;
	
	// 캐릭터 정보가 담긴 데이터 테이블 (에디터에서 할당)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Data")
	class UDataTable* CharacterDataTable;
	
	// 동적으로 생성될 캐릭터 선택 버튼들이 담길 상자 (WrapBox 또는 HorizontalBox 추천)
	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* CharacterButtonContainer;

	// 생성해낼 단일 캐릭터 버튼의 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|UI")
	TSubclassOf<class UUserWidget> CharacterSelectButtonClass;
	
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
	void OnReturnToMainClicked();
	
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
