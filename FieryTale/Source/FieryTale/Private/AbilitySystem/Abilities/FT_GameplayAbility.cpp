// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_GameplayAbility.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

const FGameplayTagContainer* UFT_GameplayAbility::GetCooldownTags() const
{
	// 1. 부모의 태그 컨테이너를 안전하게 확보 시도
	const FGameplayTagContainer* ParentTags = Super::GetCooldownTags();
    
	// 2. 만약 부모가 nullptr을 반환했다면, 엔진이 뻗지 않도록 비어있는 정적(static) 컨테이너를 우회로로 삼음
	static FGameplayTagContainer DummyContainer;
	FGameplayTagContainer* TargetContainer = ParentTags 
		? const_cast<FGameplayTagContainer*>(ParentTags) 
		: &DummyContainer;
    
	// 3. 자물쇠 태그가 안전하게 유효할 때만 컨테이너에 주입 후 반환
	if (TargetContainer && CooldownTag.IsValid())
	{
		if (TargetContainer == &DummyContainer)
		{
			DummyContainer.Reset();
		}
        
		TargetContainer->AddTag(CooldownTag);
		return TargetContainer;
	}
    
	return ParentTags;
}

void UFT_GameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}