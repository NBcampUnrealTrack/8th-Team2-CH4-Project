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
    // 자식 어빌리티들의 인스턴싱 사양 및 네트워크 마스터 가이드라인을 기저 레이어에서 선제 정렬합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
       return false;
    }

    // 궁극기 사용을 위한 게이지 최댓값 도달 여부를 실시간 속성 장부에서 검증합니다.
    if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
       if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
       {
          float CurrentGauge = AttributeSet->GetUltimateGauge();
          float MaxGauge = AttributeSet->GetMaxUltimateGauge();

          // 현재 충전된 게이지가 최댓값보다 낮거나 설정 오류로 인해 0 이하일 경우 작동을 거부합니다.
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
    // 자원 소모 검사는 CanActivateAbility의 속성 대조 파이프라인에서 선제 마감하므로 통과시킵니다.
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
        // 궁극기 코스트 소모 이펙트를 동적으로 생성하고, 현재 보유한 게이지 최댓값을 소모량으로 정밀 할당합니다.
        if (CostGameplayEffectClass)
        {
            FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
            if (CostSpecHandle.IsValid())
            {
                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                float MaxGaugeCost = AttributeSet ? AttributeSet->GetMaxUltimateGauge() : 100.0f;

                // 세팅된 궁극기 가변 소모 수치(UltimateCost)를 전용 명세 장부에 기입 밀봉합니다.
                CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::UltimateCost, MaxGaugeCost);
                ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
            }
        }
        
        // 궁극기 시전 상태를 나타내는 루즈 태그를 ASC에 즉각 등록합니다.
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
            // 안전망 개보수 수선: 비동기 타이머가 돌기 전 ASC 오브젝트를 약참조로 전면 격리 타설합니다.
            TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);

            // 0.1초 지연 소각 파이프라인: 시전 중 발생한 대미지 패킷 및 최종 정산 처리가 
            // 엔진 프레임 내에서 모두 안전하게 종료된 이후에 태그를 회수하여, 로직 꼬임을 원천 봉쇄합니다.
            FTimerHandle TagClearTimerHandle;
            World->GetTimerManager().SetTimer(TagClearTimerHandle, [WeakASC]()
            {
                // 0.1초 뒤 시점의 개체 생존 유효성을 정밀 검문하여 댕글링 크래시를 완전 방어합니다.
                if (WeakASC.IsValid())
                {
                    WeakASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
                }
            }, 0.1f, false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}