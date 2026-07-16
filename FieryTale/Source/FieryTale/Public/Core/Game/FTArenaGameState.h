// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Character/FTCharacterTypes.h"
#include "FTArenaGameState.generated.h"

class UDataTable;
struct FStreamableHandle;

/**
 * 
 */

UENUM(BlueprintType)
enum class EFTArenaMatchState : uint8
{
	WaitingToStart,
	InProgress,
	GameOver
};

UCLASS()
class FIERYTALE_API AFTArenaGameState : public AGameState
{
	GENERATED_BODY()

public:
	AFTArenaGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetArenaMatchState(EFTArenaMatchState NewState);
	
	EFTArenaMatchState GetArenaMatchState() const { return CurrentMatchState; }

	void AddTeamScore(EFTTeam ScoringTeam);

	//	서버가 킬 발생 시 호출한다. GameState는 항상 릴러번트하므로 접속 중인 모든 머신(호스트 포함)이
	//	이 멀티캐스트를 수신하고, 각자 로컬 로그 서브시스템(UFTLogSubsystem)으로 킬로그를 출력한다.
	//	완성된 표시 문자열을 그대로 넘겨 각 클라이언트는 포맷을 다시 조립하지 않는다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BroadcastKillLog(const FString& KillLogMessage);

	// UI에서 팀 스코어를 읽어갈 수 있도록 제공하는 함수
	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetBlueTeamKills() const { return BlueTeamKills; }

	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetRedTeamKills() const { return RedTeamKills; }

	// GameMode::BeginPlay()에서 세션 정원(MinPlayersToStart) 확정 직후 1회 전달받는다.
	void SetExpectedPlayerCount(int32 InExpectedPlayerCount) { ExpectedPlayerCount = InExpectedPlayerCount; }

	// 각 클라이언트가 자기 로컬 캐릭터 스폰(Possess/OnRep_PlayerState)을 마친 직후 서버에 보고한다
	// (FTPlayerController::ServerNotifyLoadingComplete에서 호출). 전원 보고가 모이면 bAllPlayersLoaded를 true로 전환한다.
	void ServerNotifyPlayerLoadingComplete(APlayerState* PS);


protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "Game State")
	EFTArenaMatchState CurrentMatchState;

	UFUNCTION()
	void OnRep_CurrentMatchState();

	// 전원이 각자 로컬 로딩을 마쳤음을 전 클라이언트에 알리는 신호. 로딩 화면 해제 전용이며 CurrentMatchState와는 독립적이다.
	UPROPERTY(ReplicatedUsing = OnRep_AllPlayersLoaded)
	bool bAllPlayersLoaded = false;

	UFUNCTION()
	void OnRep_AllPlayersLoaded();

	UPROPERTY(Replicated)
	int32 BlueTeamKills = 0;

	UPROPERTY(Replicated)
	int32 RedTeamKills = 0;

	//	게임에 존재하는 캐릭터들의 PreloadAssets를 조회할 캐릭터 데이터테이블.
	//	BP_ArenaGameState의 Class Defaults에서 DT_CharacterData를 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Preload")
	TObjectPtr<UDataTable> CharacterDataTable;

private:
	//	매치가 InProgress로 전환된 직후, 게임에 존재하는 캐릭터들의 PreloadAssets를
	//	각 머신에서 로컬로 1회 비동기 로딩한다(OnRep_CurrentMatchState에서 호출 → 서버·전 클라 실행).
	void PreloadActiveCharacterAssets();

	//	비동기 프리로드 완료 콜백 (로그용).
	void OnPreloadComplete();

	//	로드한 에셋을 매치 동안 메모리에 붙잡아두는 핸들 — 이 핸들이 살아있는 한 로드 상태가 유지되고,
	//	핸들이 소멸하면 다른 참조가 없는 에셋은 GC 대상이 된다.
	TSharedPtr<FStreamableHandle> PreloadHandle;

	//	InProgress에서 프리로드를 이미 시작했는지 (중복 실행 방지).
	bool bPreloadStarted = false;

	// 로딩 완료를 보고한 플레이어 집합 (서버 전용, 리플리케이트하지 않는다).
	TSet<TWeakObjectPtr<APlayerState>> LoadingCompletePlayers;

	// 매치에 참여할 총 인원수. GameMode가 세션 정원을 계산한 뒤 SetExpectedPlayerCount로 전달한다.
	int32 ExpectedPlayerCount = 0;

	// 전원 보고 완료 후 실제 bAllPlayersLoaded 신호를 내보내기까지의 유예 시간(초).
	// 캐릭터 possess 직후에도 텍스처/셰이더 스트리밍이 마저 끝나도록 약간의 여유를 둔다.
	static constexpr float AllPlayersLoadedBroadcastDelay = 5.0f;

	// 유예 타이머 핸들.
	FTimerHandle AllPlayersLoadedDelayTimer;

	// 타이머 만료 후 실제로 bAllPlayersLoaded를 true로 전환하고 리플리케이션을 트리거한다.
	void BroadcastAllPlayersLoaded();

};
