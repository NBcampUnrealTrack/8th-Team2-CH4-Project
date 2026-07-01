#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FTGameMode.generated.h"

class AFTTurret;
class AFTNexus;

UCLASS()
class FIERYTALE_API AFTGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	// 임시로 캐릭터 Spawn하는 코드 작성해두었습니다.
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 포탑이 파괴되었을 때 진영과 위치 정보를 받아 로그를 출력하는 함수
	void TurretDestroyed(AFTTurret* DestroyedTurret);

	// 넥서스가 파괴되었을 때 게임 종료 절차를 트리거하는 함수
	void NexusDestroyed(AFTNexus* DestroyedNexus);

protected:
	// 연산된 승리 팀 정보를 기반으로 최종 처리를 수행하는 함수
	void HandleGameOver(uint8 WinningTeam);
};