#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Character/FTPlayerState.h" // 캐릭터 타입 Enum 및 PlayerState 참조
#include "FTGameMode.generated.h"

class AFTTurret;
class AFTNexus;
class UUserWidget;

UCLASS()
class FIERYTALE_API AFTGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AFTGameMode();

	virtual void BeginPlay() override;
	
	// 일반 트래블을 통해 플레이어가 새 레벨에 완전히 정착했을 때 호출되는 핵심 함수
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	
	// 심리스 트래블로 넘어온 플레이어를 처리하는 함수 오버라이드
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	
	void InitializeAndSpawnPlayer(APlayerController* NewPlayer);

protected:
	
	// 세션에서 받아올 필수 시작 인원수
	UPROPERTY(BlueprintReadOnly, Category = "Game Settings")
	int32 MinPlayersToStart = 2;

	// 매치 종료(넥서스 파괴) 시 표시할 결과 UI 위젯 클래스.
	// nullptr이면 결과 보고 없이 세션(연결)을 해산하고 메인 레벨로 복귀한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FieryTale|GameOver")
	TSubclassOf<UUserWidget> ResultWidgetClass;

	// 🌟 추가: 인원이 모두 차면 전장 전투를 시작하는 함수
	void StartArenaMatch();


public:

	// 포탑이 파괴되었을 때 진영과 위치 정보를 받아 로그를 출력하는 함수
	void TurretDestroyed(AFTTurret* DestroyedTurret);

	// 넥서스가 파괴되었을 때 게임 종료 절차를 트리거하는 함수
	void NexusDestroyed(AFTNexus* DestroyedNexus);
	
	// 연산된 승리 팀 정보를 기반으로 최종 처리를 수행하는 함수
	void HandleGameOver(uint8 WinningTeam);
};