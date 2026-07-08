#include "Level/FTNexus.h"
#include "GameplayTags/FTTags.h"
#include "Core/Game/FTGameMode.h"
#include "Engine/World.h"

AFTNexus::AFTNexus()
{
	NexusTeam = EFTNexusTeam::None;
}

FGameplayTag AFTNexus::GetTeamTag() const
{
	return (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
}

FGameplayTag AFTNexus::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Nexus;
}

void AFTNexus::NotifyDestroyed()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->NexusDestroyed(this);
		}
	}
}
