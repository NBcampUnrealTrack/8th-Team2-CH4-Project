// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/FTBattleSubsystem.h"
#include "Level/FTNexus.h"
#include "FieryTaleLog.h"
#include "EngineUtils.h"

AFTNexus* UFTBattleSubsystem::GetNexus(EFTTurretTeam Team) const
{
	const EFTNexusTeam TargetTeam = (Team == EFTTurretTeam::BlueTeam) ? EFTNexusTeam::BlueTeam : EFTNexusTeam::RedTeam;

	for (TActorIterator<AFTNexus> It(GetWorld()); It; ++It)
	{
		if (It->GetNexusTeam() == TargetTeam)
		{
			return *It;
		}
	}

	return nullptr;
}

void UFTBattleSubsystem::NotifyTurretDestroyed(AFTTurret* DestroyedTurret)
{
	if (!DestroyedTurret)
	{
		return;
	}

	const EFTTurretTeam Team = DestroyedTurret->GetTurretTeam();
	const int32 NewCount = ++DestroyedTurretCounts.FindOrAdd(Team);

	if (NewCount < RequiredDestroyedTurretsToUnlockNexus)
	{
		return;
	}

	AFTNexus* Nexus = GetNexus(Team);
	if (!Nexus)
	{
		return;
	}

	Nexus->SetVulnerable(true);

	UE_LOG(LogFTSession, Log, TEXT("[Battle] %s팀 타워 %d개 파괴 → 넥서스가 파괴 가능 상태로 전환되었습니다."),
		(Team == EFTTurretTeam::BlueTeam) ? TEXT("Blue") : TEXT("Red"), NewCount);
}
