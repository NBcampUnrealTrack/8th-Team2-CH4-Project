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
	bTeamAssigned = true; // false→true는 항상 실제 변화이므로, 이 값을 트리거로 삼아 클라이언트 RepNotify를 보장한다
	ApplyTeamTag(); // 서버(Authority) 자신은 OnRep을 안 타므로 여기서 바로 적용
}

void AFTPlayerState::AddKill()
{
	if (HasAuthority()) Kills++;
}

void AFTPlayerState::AddDeath()
{
	if (HasAuthority()) Deaths++;
}

void AFTPlayerState::OnRep_TeamAssigned()
{
	ApplyTeamTag(); // 이 시점엔 Team 값도 이미 서버가 배정한 최종값으로 도착해 있다
}

void AFTPlayerState::ApplyTeamTag()
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

	// 팀 태그 확정을 기다리던 체력바 위젯들에게 통지하고, 1회성 신호이므로 즉시 비운다
	OnTeamTagReady.Broadcast();
	OnTeamTagReady.Clear();
}

void AFTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AFTPlayerState, SpawnedCharacter);
	DOREPLIFETIME(AFTPlayerState, Team);
	DOREPLIFETIME(AFTPlayerState, bTeamAssigned);
	DOREPLIFETIME(AFTPlayerState, SelectedCharacterType);
	DOREPLIFETIME(AFTPlayerState, PlayerIndex);
	DOREPLIFETIME(AFTPlayerState, Kills);
	DOREPLIFETIME(AFTPlayerState, Deaths);
}

