#include "Character/FTPlayerState.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

AFTPlayerState::AFTPlayerState()
{
	bReplicates = true;

	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AFTPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFTPlayerState::CopyProperties(APlayerState* PlayerState)
{
	// FTLobbyPlayerState로 부터 정보를 받아오는 함수
	Super::CopyProperties(PlayerState);
}

void AFTPlayerState::AssignTeamTag(EFTTeam InTeam)
{
	Team = InTeam;
	ApplyTeamTag();
}

void AFTPlayerState::AddKill()
{
	if (HasAuthority()) Kills++;
}

void AFTPlayerState::AddDeath()
{
	if (HasAuthority()) Deaths++;
}

void AFTPlayerState::OnRep_Team()
{
	ApplyTeamTag();
}

void AFTPlayerState::ApplyTeamTag() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(FTTags::FTFaction::Team_Blue);
	ASC->RemoveLooseGameplayTag(FTTags::FTFaction::Team_Red);

	const FGameplayTag& TeamTag = (Team == EFTTeam::Blue)
		? FTTags::FTFaction::Team_Blue
		: FTTags::FTFaction::Team_Red;

	ASC->AddLooseGameplayTag(TeamTag);
}

void AFTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AFTPlayerState, SpawnedCharacter);
	DOREPLIFETIME(AFTPlayerState, Team);
	DOREPLIFETIME(AFTPlayerState, SelectedCharacterType);
	DOREPLIFETIME(AFTPlayerState, PlayerIndex);
	DOREPLIFETIME(AFTPlayerState, Kills);
	DOREPLIFETIME(AFTPlayerState, Deaths);
}

