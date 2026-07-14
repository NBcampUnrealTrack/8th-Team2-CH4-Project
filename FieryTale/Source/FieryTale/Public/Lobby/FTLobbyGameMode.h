// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Character/FTCharacterTypes.h"
#include "FTLobbyGameMode.generated.h"

class UWorld;

/**
 * 대기방(Lobby 맵) 전용 GameMode.
 *
 * 접속/이탈을 추적하고, 준비(Ready) 조건이 충족되면 매치(아레나) 맵으로 ServerTravel 한다.
 * 세션 생성/검색/입장 자체는 UFTSessionSubsystem 이 담당하고, 이 클래스는
 * "대기방에 모인 사람들"의 시작 흐름만 관리한다.
 */
UCLASS()
class FIERYTALE_API AFTLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFTLobbyGameMode();
	
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	

	/** 어떤 플레이어의 Ready 상태가 바뀌었을 때 PlayerController(서버)가 호출한다. */
	void NotifyReadyStateChanged();

	/** 호스트가 명시적으로 매치 시작을 요청했을 때 호출한다. */
	void RequestStartMatch(APlayerController* Requester);

protected:
	/** 매치(아레나) 맵. 에디터/BP에서 레벨 에셋을 직접 지정한다.
	 *  비어 있으면 코드 폴백(/Game/Maps/L_Arena)을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FieryTale|Lobby")
	TSoftObjectPtr<UWorld> GameLevel;

	/** 시작에 필요한 최소 인원. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FieryTale|Lobby")
	int32 MinPlayersToStart = 2;

	/** 전원 Ready 시 자동으로 매치를 시작할지 여부. false 면 호스트 버튼으로만 시작. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FieryTale|Lobby")
	bool bAutoStartWhenAllReady = false;

	bool AreAllPlayersReady() const;
	void TravelToMatch();

	bool bMatchStarting = false;
};
