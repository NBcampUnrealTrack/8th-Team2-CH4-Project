// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_GameplayAbility.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모 클래스의 ActivateAbility를 호출합니다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

const FGameplayTagContainer* UFT_GameplayAbility::GetCooldownTags() const
{
	// 인스턴스 고유의 쿨다운 태그 컨테이너를 반환합니다.
	const FGameplayTagContainer* ParentTags = Super::GetCooldownTags();
    
	// 지정된 쿨타임 태그가 유효하지 않으면 부모의 반환값을 사용합니다.
	if (!CooldownTag.IsValid())
	{
		return ParentTags;
	}

	// 인스턴스 고유의 멤버 변수를 수정하기 위해 임시로 상수성을 해제합니다.
	UFT_GameplayAbility* MutableThis = const_cast<UFT_GameplayAbility*>(this);
    
	// 부모 클래스의 공용 쿨다운 태그를 복사합니다.
	FGameplayTagContainer CombinedTags;
	if (ParentTags)
	{
		CombinedTags.AppendTags(*ParentTags);
	}

	// 지정된 쿨타임 태그를 결합합니다.
	CombinedTags.AddTag(CooldownTag);

	// 최종 결합된 컨테이너를 멤버 변수인 RuntimeCooldownTags에 대입하여 반환합니다.
	MutableThis->RuntimeCooldownTags = CombinedTags;
    
	return &RuntimeCooldownTags;
}

void UFT_GameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 런타임에 결합된 쿨다운 태그를 기반으로 쿨타임을 적용합니다.
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}