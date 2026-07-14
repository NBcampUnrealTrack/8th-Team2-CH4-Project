// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Level/FTTurret.h"
#include "FTBattleSubsystem.generated.h"

class AFTNexus;

/**
 * 매치 단위 전장 상태(타워/넥서스) 조회 및 파괴 이벤트 집계를 담당하는 월드 서브시스템.
 * 타워/넥서스는 레벨에 정적으로 배치되므로 별도의 Register/Unregister 없이
 * 필요할 때마다 TActorIterator로 직접 조회한다.
 */
UCLASS()
class FIERYTALE_API UFTBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Battle")
	TArray<AFTTurret*> GetTurrets(EFTTurretTeam Team) const;

	// 해당 팀의 넥서스를 레벨에서 찾아 반환한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Battle")
	AFTNexus* GetNexus(EFTTurretTeam Team) const;

	// 타워 파괴 이벤트를 접수해 팀별 파괴 누적치를 갱신하고,
	// 임계치(RequiredDestroyedTurretsToUnlockNexus) 도달 시 해당 팀 넥서스의 파괴 가능 처리를 트리거한다.
	void NotifyTurretDestroyed(AFTTurret* DestroyedTurret);

private:
	// 팀 소속 넥서스가 파괴 가능해지기 위해 필요한 최소 타워 파괴 수.
	// 이 값만 바꾸면 조건이 바뀌므로 GameMode/Turret/Nexus는 다시 건드릴 필요가 없다.
	int32 RequiredDestroyedTurretsToUnlockNexus = 1;

	// 팀별 누적 파괴 타워 수.
	TMap<EFTTurretTeam, int32> DestroyedTurretCounts;
};
