// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_KaguyaChargeSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaChargeSkill::UFT_KaguyaChargeSkill()
    : ChargeSpeed(1800.0f)
    , ChargeDuration(0.4f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // 돌진 중에는 밀쳐내기에 면역 상태임을 알리는 태그를 부여합니다
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.ImmuneToKnockback")));
}

void UFT_KaguyaChargeSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character) return;

    // 가구야는 항상 전방(캐릭터 정면)으로 강력하게 돌진합니다
    FVector ChargeDirection = Character->GetActorForwardVector();

    UAbilityTask_ApplyRootMotionConstantForce* ChargeTask = 
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this, FName("KaguyaCharge"), ChargeDirection, ChargeSpeed, ChargeDuration, 
            false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f, false);

    if (ChargeTask)
    {
        ChargeTask->OnFinish.AddDynamic(this, &UFT_KaguyaChargeSkill::OnChargeFinished);
        ChargeTask->ReadyForActivation();
    }
    
    // 추후 구현 포인트 돌진 경로 상에 있는 적 감지(Sweep) 및 히트 시 밀쳐내기 힘(Impulse) 적용 구역
}

void UFT_KaguyaChargeSkill::OnChargeFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaChargeSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!bWasCancelled) CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}