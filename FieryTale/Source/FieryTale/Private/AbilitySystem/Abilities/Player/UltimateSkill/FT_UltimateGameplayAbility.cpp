// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTags/FTTags.h"

UFT_UltimateGameplayAbility::UFT_UltimateGameplayAbility()
{
    // 궁극기는 고유 인풋 태그 또는 스킬 분류 태그로 관리됩니다.
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // GAS 순정 시스템 검증: 쿨타임, 침묵, 기절 등 공용 상태 이상 태그 검문선 통과
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
       return false;
    }

    // 기획 반영: 일반 마나 소모 방식이 아닌 소유자의 AttributeSet을 찾아 현재 궁극기 게이지가 최대치에 달성했는지 정밀 검증합니다.
    if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
       if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
       {
          float CurrentGauge = AttributeSet->GetUltimateGauge();
          float MaxGauge = AttributeSet->GetMaxUltimateGauge();

          // 게이지가 가득 차지 않았거나 최대 게이지 비정상 설정 시 시전을 원천 차단합니다.
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
    //  [순정 오버라이드 원칙]: 부모 클래스의 마나/스태미나 자원 차감 선로를 전면 끄고 
    // 우리의 순정 UltimateGauge 전용 커스텀 차감 배관으로 대체 가동합니다.
    // Super::ApplyCost를 호출하지 않음으로써 마나 GE의 중복 격발을 완벽 차단합니다.

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC) return;

    if (CostGameplayEffectClass)
    {
        FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
        if (CostSpecHandle.IsValid())
         {
            // =========================================================================
            //  [비용 소모 누수 완치 완착]: 매그니튜드에 0.0f를 주입하던 오류를 청소하고,
            // 에디터 단에서 수치를 어떻게 세팅하든 C++ 강제 주입선에서 최대치 전량 소모 명세인
            // '100.0f' 정수(혹은 AttributeSet 연산 규칙에 맞춘 음수 부호)를 실어 보냅니다.
            // =========================================================================
            const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            float MaxGaugeCost = AttributeSet ? AttributeSet->GetMaxUltimateGauge() : 100.0f;

            // SetByCaller 비용 패킷 장부에 소모할 총량 스펙을 굳건하게 락인합니다.
            CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::UltimateCost, MaxGaugeCost);
            
            // 시전자의 능력 시스템 컴포넌트에 직통 사출하여 자원 소모 동기화를 격발합니다.
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
        }
    }
}