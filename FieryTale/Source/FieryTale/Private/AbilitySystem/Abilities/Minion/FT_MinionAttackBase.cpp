// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_MinionAttackBase.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "AbilitySystemInterface.h" // [필수 헤더선] IAbilitySystemInterface 참조를 위해 추가

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // [GAS 마스터 전투 CC 가드선 완착]
    // 기절(Stunned) 상태가 본체 ASC에 걸려있다면 엔진 레벨에서 공격 격발을 원천 차단합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

void UFT_MinionAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar || !AttackMontage)
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // C++ 순정 PlayMontageAndWait 태스크 동적 생성 및 애니메이션 시동
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        TEXT("MinionAttackTask"),
        AttackMontage,
        1.0f,
        NAME_None,
        false
    );

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnBlendOut.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        
        MontageTask->ReadyForActivation();
    }
    else
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // =========================================================================
    // 플레이스홀더로 비어있던 블록을 채우고, AI 컨트롤러가 타겟팅 중인 대상에게 
    // 기획자가 설정한 DamageEffectClass 계산서를 최종 주입합니다.
    // =========================================================================
    if (AAIController* AIC = Cast<AAIController>(AvatarChar->GetController()))
    {
        UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
        
        // 브레인이 심어준 추격/교전 대상(BestTarget)의 주소지를 AI 레이더에서 재확보합니다.
        if (MyASC && DamageEffectClass)
        {
            if (AActor* TargetActor = AIC->GetFocusActor())
            {
                // 상대방 액터가 GAS 컴포넌트를 보유하고 있는지 순정 인터페이스 검증을 거칩니다.
                if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(TargetActor))
                {
                    if (UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent())
                    {
                        // 내 능력치 정보가 담긴 Context 인프라 생성
                        FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
                        EffectContext.AddSourceObject(AvatarChar);

                        // DamageEffectClass 기반 실시간 연산서(Spec) 발행
                        FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
                        if (NewSpecHandle.IsValid())
                        {
                            // 타겟의 장부에 최종 데미지 반영 처리
                            MyASC->ApplyGameplayEffectSpecToTarget(*NewSpecHandle.Data.Get(), TargetASC);
                        }
                    }
                }
            }
        }
    }
}

void UFT_MinionAttackBase::OnMontageTargetedEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
    // 향후 원거리 투사체 스폰 시점 분기용 가드선
}

void UFT_MinionAttackBase::OnMontageCompletedOrCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_MinionAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}