#include "Character/FTPlayerState.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"

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

void AFTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}