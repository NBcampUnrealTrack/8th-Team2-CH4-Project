// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "AbilitySystem/FT_AttributeSet.h"


UFT_UltimateGameplayAbility::UFT_UltimateGameplayAbility()
{
	// 궁극기는 고유 인풋 태그 또는 스킬 분류 태그로 관리됩니다
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// [기획 반영] 소유자의 AttributeSet을 찾아 현재 궁극기 게이지가 최대치에 도달했는지 정밀 검증합니다
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
		{
			float CurrentGauge = AttributeSet->GetUltimateGauge();
			float MaxGauge = AttributeSet->GetMaxUltimateGauge();

			// 게이지가 가득 차지 않았다면 시전을 원천 차단합니다 (UI 연동용)
			if (CurrentGauge < MaxGauge || MaxGauge <= 0.0f)
			{
				return false;
			}
		}
	}

	return true;
}

void UFT_UltimateGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Super::ApplyCost를 우회하고 궁극기만의 독자적인 게이지 리셋 룰을 네이티브로 강제 격발합니다
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (UFT_AttributeSet* AttributeSet = const_cast<UFT_AttributeSet*>(Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()))))
		{
			// [기획 반영] 궁극기 시전 즉시 게이지를 0.0f으로 완전 청소
			AttributeSet->SetUltimateGauge(0.0f);
		}
	}
}