// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)
{
    // 인스턴싱 및 네트워크 실행 정책을 로컬 예측 규격으로 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 고유 자산 태그 등록 및 쿨타임 추적용 태그 매핑을 수행합니다.
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

    // [쿨타임 완치 배관]: 이륙 시점에 CommitAbility 마스터 함수를 확실하게 격발하여 쿨타임 GE 장부를 안착시킵니다.
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
    
    // 비행 중 속도 30% 감산 페널티 적용: 순정 GE 배관망을 활용해 복합 연산 호환성을 확보합니다.
    if (FlyMovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(FlyMovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            FlyMovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }
    
    // 무브먼트 모드를 비행으로 안전 변환 및 부력 격발
    MoveComp->SetMovementMode(EMovementMode::MOVE_Flying);
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
    // 5초 만료 시 정상 착륙 종료선 진입
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
        // 비행 종료 시 항상 순정 걷기 모드로 복귀 조치
        Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
    }

    if (SourceASC)
    {
        // 이속 페널티 GE를 철거하여 속도 스탯 자동 환원
        if (FlyMovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(FlyMovementPenaltyActiveHandle);
            FlyMovementPenaltyActiveHandle.Invalidate();
        }

        // [네트워크 동기화 쿨타임 환불]: CC기나 인터럽트로 취소된 경우, 표준 쿼리 배관으로 쿨타임 이펙트를 즉각 회수합니다.
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            // 엔진 공식 표준 쿼리 배관(MakeQuery_MatchAnyOwningTags)으로 교체하여 최적화 및 안정성 확보
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}