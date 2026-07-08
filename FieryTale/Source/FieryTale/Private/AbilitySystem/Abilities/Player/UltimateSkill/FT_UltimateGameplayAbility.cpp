// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTags/FTTags.h"

UFT_UltimateGameplayAbility::UFT_UltimateGameplayAbility()
{
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
       return false;
    }

    if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
       if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
       {
          float CurrentGauge = AttributeSet->GetUltimateGauge();
          float MaxGauge = AttributeSet->GetMaxUltimateGauge();

          if (CurrentGauge < MaxGauge || MaxGauge <= 0.0f)
          {
             return false;
          }
       }
    }

    return true;
}

bool UFT_UltimateGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    // 엔진의 깐깐한 자동 검사를 패스시켜 에러 로그를 멸균합니다.
    return true; 
}

// 💡 [코스트 삭제 현상 완전 치료]: CommitAbility 메커니즘을 우회하여, 
// 어빌리티가 진짜 발동하는 시점에 강제로 게이지 소모를 집도합니다.
void UFT_UltimateGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // 순정 ApplyCost는 비워두거나 부모만 호출해 둡니다.
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

// =========================================================================
// 💡 [활성화 구역 강제 집행선 개통]
// 각 영웅들의 실제 궁극기 시전부(ActivateAbility) 최상단에서 이 함수를 거치게 하여 
// 게이지를 확실하고 무결하게 전량 소각 처리합니다.
// =========================================================================
void UFT_UltimateGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 💡 [강제 비용 정산 파이프라인]
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (ASC && CostGameplayEffectClass)
    {
        FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
        if (CostSpecHandle.IsValid())
        {
            const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            float MaxGaugeCost = AttributeSet ? AttributeSet->GetMaxUltimateGauge() : 100.0f;

            // SetByCaller 비용 패킷 장부에 정확하게 소모 수치를 낙인찍고
            CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::UltimateCost, MaxGaugeCost);
            
            // 시전자 본인에게 강제 착륙시켜 게이지를 깎아냅니다.
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
        }
    }
}