// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h" // ◄ ASC 상호작용을 위해 추가 완착
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 활성화 기간 동안 알라딘 고유 비행 버프 태그를 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Flying);
}

void UFT_AladdinFlySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 1단계: 시프트 생존기 쿨타임 검문
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // =========================================================================
    // [쿨타임 완치 배관]: 이륙 시점에 CommitAbility 마스터 함수를 확실하게 격발합니다.
    // 이를 통해 우클릭/시프트 공용 Cooldown GE 장부가 시스템에 안착합니다.
    // =========================================================================
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!Character || !MoveComp || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    // =========================================================================
    // 💡 [속도 누수 완치 - GAS 순정 이속 페널티 라인 연동]
    // 변수를 직접 조지는 하드코딩을 소각하고, 다른 버프/디버프와 복합 연산이 원활하도록
    // 기획서 명세(비행 중 속도 30% 감산)가 세팅된 순정 GE를 내 몸에 투입합니다.
    // =========================================================================
    if (FlyMovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(FlyMovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            FlyMovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }
    
    // 무브먼트 모드를 비행으로 안전 변환
    MoveComp->SetMovementMode(EMovementMode::MOVE_Flying);

    // 공중 부력 물리 벡터 순간 격발
    Character->LaunchCharacter(FVector(0.0f, 0.0f, 250.0f), false, true);
    
    // 5초 지속 제한 시간 타이머 가동
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            FlyDurationTimerHandle, 
            this, 
            &UFT_AladdinFlySkill::OnFlyDurationExpired, 
            FlyDuration, 
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AladdinFlySkill::OnFlyDurationExpired()
{
    // 5초 만료 시 정상 착륙 종료선 진입 (bWasCancelled = false)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinFlySkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 청정 소각
    if (GetWorld() && FlyDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(FlyDurationTimerHandle);
        FlyDurationTimerHandle.Invalidate();
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character && Character->GetCharacterMovement())
    {
        // 하드 CC나 타이머 만료 등 어떤 경로로든 비행이 종료되면 무조건 순정 걷기 모드로 복귀 조치
        Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
    }

    if (SourceASC)
    {
        // 💡 비행 종료 시점에 이속 페널티 GE를 깔끔하게 철거하여 속도를 원래 스탯대로 자동 환원합니다.
        if (FlyMovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(FlyMovementPenaltyActiveHandle);
            FlyMovementPenaltyActiveHandle.Invalidate();
        }

        // =========================================================================
        // [AOS 기획 사양 대응]: 양탄자 비행 도중 적의 하드 CC기에 억까당해 취소당했다면(bWasCancelled),
        // 선제 적용되었던 시프트 쿨타임 이펙트를 찾아내어 장부에서 무결하게 환불 복구합니다.
        // 정상 수명 만료로 종료되었다면 쿨타임이 그대로 유지됩니다.
        // =========================================================================
        if (bWasCancelled)
        {
            FGameplayEffectQuery UniversalQuery;
            TArray<FActiveGameplayEffectHandle> ActiveHandles = SourceASC->GetActiveEffects(UniversalQuery);

            for (const FActiveGameplayEffectHandle& GEPipeHandle : ActiveHandles)
            {
                const FActiveGameplayEffect* ActiveGE = SourceASC->GetActiveGameplayEffect(GEPipeHandle);
                if (ActiveGE && ActiveGE->Spec.Def)
                {
                    if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                        ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                    {
                        SourceASC->RemoveActiveGameplayEffect(GEPipeHandle);
                    }
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}