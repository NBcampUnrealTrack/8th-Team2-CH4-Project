// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Engine/World.h"

UFT_UltimateGameplayAbility::UFT_UltimateGameplayAbility()
{
    // 인스턴싱 및 네트워크 실행 정책을 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
       return false;
    }

    // 궁극기 게이지가 가득 찼는지 확인합니다.
    if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
       if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
       {
          float CurrentGauge = AttributeSet->GetUltimateGauge();
          float MaxGauge = AttributeSet->GetMaxUltimateGauge();

          // 게이지가 부족하거나 최댓값이 0 이하인 경우 활성화를 거부합니다.
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
    // 코스트 검증은 CanActivateAbility에서 처리하므로 여기서는 무조건 통과시킵니다.
    return true; 
}

void UFT_UltimateGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (ASC && CostGameplayEffectClass)
    {
        FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
        if (CostSpecHandle.IsValid())
        {
            const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            float MaxGaugeCost = AttributeSet ? AttributeSet->GetMaxUltimateGauge() : 100.0f;

            // 소모량을 설정합니다.
            CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::UltimateCost, MaxGaugeCost);
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
        }
    }
}

void UFT_UltimateGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (ASC)
    {
        // 어빌리티 활성화 시 궁극기 상태 태그를 부여합니다.
        ASC->AddLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }
}

void UFT_UltimateGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        UWorld* World = ASC->GetWorld();
        
        if (World)
        {
            // ASC를 약참조로 저장합니다.
            TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);

            // 지연 후 궁극기 상태 태그를 제거합니다.
            FTimerHandle TagClearTimerHandle;
            World->GetTimerManager().SetTimer(TagClearTimerHandle, [WeakASC]()
            {
                // 유효성을 검증하고 태그를 제거합니다.
                if (WeakASC.IsValid())
                {
                    WeakASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
                }
            }, 0.1f, false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}