#include "Level/FTTurret.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"
#include "Core/Game/FTGameMode.h"
#include "Engine/World.h"

AFTTurret::AFTTurret()
{
	TurretTeam = EFTTurretTeam::BlueTeam;
	TurretPosition = EFTTurretPosition::None;
}

FGameplayTag AFTTurret::GetTeamTag() const
{
	return (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
}

FGameplayTag AFTTurret::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Turret;
}

void AFTTurret::NotifyDestroyed()
{
	//	파괴된 포탑은 더 이상 행동하지 못하도록 뮤트 태그 부여 (기존 동작 유지)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted);
	}

	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->TurretDestroyed(this);
		}
	}
}
