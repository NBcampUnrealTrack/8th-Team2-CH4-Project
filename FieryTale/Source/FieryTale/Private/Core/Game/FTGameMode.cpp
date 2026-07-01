#include "Core/Game/FTGameMode.h"
#include "Level/FTTurret.h"
#include "Level/FTNexus.h"
#include "FieryTaleLog.h"

void AFTGameMode::TurretDestroyed(AFTTurret* DestroyedTurret)	// 포탑 파괴 이벤트 처리 로직 구현
{
	if (!DestroyedTurret)										// 안전검사
	{
		return;
	}

	EFTTurretTeam Team = DestroyedTurret->GetTurretTeam();		// 소스 액터에 구현된 게터를 통해 파괴된 대상 소속 진영 수신
	EFTTurretPosition Position = DestroyedTurret->GetTurretPosition();	// 배치 위치 수신

	FString TeamStr = (Team == EFTTurretTeam::BlueTeam) ? TEXT("Blue Team") : TEXT("Red Team");
	// 진영 열거형 값이 블루팀인 경우와 그 외의 경우를 삼항연산자로 판별해 문자열로 치환 (로직만 구현하고 임시로 문자열 출력)
	FString PosStr = (Position == EFTTurretPosition::Left) ? TEXT("Left") : TEXT("Right");
	// 왼쪽 라인인지 오른쪽 라인인지도 구별

	UE_LOG(LogTemp, Log, TEXT("Notification: %s's %s Turret has been destroyed!"), *TeamStr, *PosStr);
	// 포맷 스트링 형식을 빌려 어떤 팀의 어느 쪽 라인 방어 타워가 파괴되었는지 출력 창에 실시간 기록
}

void AFTGameMode::NexusDestroyed(AFTNexus* DestroyedNexus)
{
	if (!DestroyedNexus)
	{
		return;
	}

	uint8 LosingTeam = static_cast<uint8>(DestroyedNexus->GetNexusTeam());
	uint8 WinningTeam = (LosingTeam == 1) ? 2 : 1;
	HandleGameOver(WinningTeam);
}

void AFTGameMode::HandleGameOver(uint8 WinningTeam)
{
	if (WinningTeam == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("Blue Team Wins"));
	}
	else if (WinningTeam == 2)
	{
		UE_LOG(LogTemp, Log, TEXT("Red Team Wins"));
	}
}