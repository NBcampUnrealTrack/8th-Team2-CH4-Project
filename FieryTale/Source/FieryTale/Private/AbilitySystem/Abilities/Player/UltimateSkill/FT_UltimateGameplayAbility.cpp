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
    return true; 
}

void UFT_UltimateGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

void UFT_UltimateGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (ASC)
    {
        if (CostGameplayEffectClass)
        {
            FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
            if (CostSpecHandle.IsValid())
            {
                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                float MaxGaugeCost = AttributeSet ? AttributeSet->GetMaxUltimateGauge() : 100.0f;

                CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::UltimateCost, MaxGaugeCost);
                ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
            }
        }
        
        // 💡 [순정 API 교정 완착]: 컴파일러가 완벽히 해석 가능한 순정 루즈 태그 삽입선으로 복구합니다.
        ASC->AddLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }
}

// =========================================================================
// 💡 [지연 소각 가드선 오버라이드 완공]
// =========================================================================
void UFT_UltimateGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        UWorld* World = ASC->GetWorld();
        
        if (World)
        {
            // 💡 [0.1초 지연 소각 파이프라인]: 순정 RemoveLooseGameplayTag 장부를 
            // 안전하게 캡처하여 대미지 패킷이 정산소를 완전히 빠져나간 뒤 회수 처리합니다.
            FTimerHandle TagClearTimerHandle;
            World->GetTimerManager().SetTimer(TagClearTimerHandle, [ASC]()
            {
                if (ASC)
                {
                    ASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
                }
            }, 0.1f, false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}