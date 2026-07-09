// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_KaguyaBulwarkAbility.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaBulwarkAbility::UFT_KaguyaBulwarkAbility()
    : MaxDuration(4.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;

    // 활성화 기간 동안 가구야 반격 가드 태세 태그를 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::CounterReady);
}

void UFT_KaguyaBulwarkAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1단계: 우클릭 쿨타임 검문
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // =========================================================================
    // [쿨타임 완치 배관]: 방벽이 전개되는 확실한 시점에 CommitAbility 마스터 함수를 격발합니다.
    // =========================================================================
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!Character || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 채널링 감시 태그 가동
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    
    // 이동속도 저하 페널티 GE 주입
    if (MovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }

    // 가드 모션 애니메이션 몽타주 가동
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    if (WeaponData && WeaponData->AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), WeaponData->AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 애니메이션이 끝나도 EndAbility를 호출하지 않고 태세 유지를 위해 홀딩합니다.
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardInterrupted);
            MontageTask->ReadyForActivation();
        }
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            BulwarkDurationTimerHandle, 
            this, 
            &UFT_KaguyaBulwarkAbility::ExpireBulwark, 
            MaxDuration, 
            false
        );
    }
}

void UFT_KaguyaBulwarkAbility::ExpireBulwark()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaBulwarkAbility::OnGuardInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_KaguyaBulwarkAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 청정 소각
    if (GetWorld() && BulwarkDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
        BulwarkDurationTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 채널링 및 이속 페널티 이펙트 장부 원점 제거
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
        
        if (MovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
            MovementPenaltyActiveHandle.Invalidate();
        }

        // CC기 취소 분기 시 선제 적용된 쿨타임 태그가 있다면 무결하게 환불 복구합니다.
        if (bWasCancelled)
        {
            FGameplayEffectQuery UniversalQuery;
            TArray<FActiveGameplayEffectHandle> ActiveHandles = SourceASC->GetActiveEffects(UniversalQuery);

            // [메모리 컨테이너 무결성 타설망]:
            // 실시간 제거 루프를 파쇄하고 제거 대상 타깃 핸들들만 격리 어레이에 깨끗하게 선제 수집합니다.
            TArray<FActiveGameplayEffectHandle> HandlesToRemove;

            for (const FActiveGameplayEffectHandle& GEPipeHandle : ActiveHandles)
            {
                const FActiveGameplayEffect* ActiveGE = SourceASC->GetActiveGameplayEffect(GEPipeHandle);
                if (ActiveGE && ActiveGE->Spec.Def)
                {
                    if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                        ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                    {
                        HandlesToRemove.Add(GEPipeHandle);
                    }
                }
            }

            // 안전 구역 탈출 직후 수집 장부를 기반으로 원자적 소각을 완수하여 서버 크래시를 차단합니다.
            for (const FActiveGameplayEffectHandle& TargetHandle : HandlesToRemove)
            {
                SourceASC->RemoveActiveGameplayEffect(TargetHandle);
            }
        }
    }
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}