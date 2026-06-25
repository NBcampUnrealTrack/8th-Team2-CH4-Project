// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"


void UFT_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	HandleAutoActivatedAbility(AbilitySpec);
}

void UFT_AbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	
	FScopedAbilityListLock ActiveScopesLock(*this);
	
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		HandleAutoActivatedAbility(AbilitySpec);
	}
}

void UFT_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (IsValid(GetAvatarActor()) && !GetAvatarActor()-> HasAuthority())
	{
		return;
	}
	
	if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
	{
		AbilitySpec-> Level = Level;
		MarkAbilitySpecDirty(*AbilitySpec);
	}
}

void UFT_AbilitySystemComponent::AddToAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (IsValid(GetAvatarActor()) && !GetAvatarActor()-> HasAuthority())
	{
		return;
	}
	
	if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
	{
		AbilitySpec-> Level += Level;
		MarkAbilitySpecDirty(*AbilitySpec);
	}
}

void UFT_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	if (!IsValid(AbilitySpec.Ability))
	{
		return;
	}
	
	for (const FGameplayTag& Tag :AbilitySpec.Ability->GetAssetTags())
	{
		if (Tag.MatchesTagExact(FTTags::FTAbilities::ActivateOnGiven))
		{
			TryActivateAbility(AbilitySpec.Handle); 
			return;
		}
	}
}
