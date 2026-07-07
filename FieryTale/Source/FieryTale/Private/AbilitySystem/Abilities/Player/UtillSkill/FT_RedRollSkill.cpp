// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_RedRollSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameplayTags/FTTags.h"

UFT_RedRollSkill::UFT_RedRollSkill()
    : RollSpeed(1500.0f)     
    , RollDuration(0.35f)    
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Evading);
}

void UFT_RedRollSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 마나 자원이 없으므로 현재 15초 시프트 쿨타임 태그가 걸려있는지만 순정 GAS 관문에서 체크합니다.
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 플레이어가 입력 중인 방향 패킷을 분석하여 순정 물리 이동 방향을 확보합니다.
    FVector RollDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();

    // 만약 키보드 입력이 일절 없는 정지 상태라면 영웅의 정면 기준 180도 반대인 후방 구르기로 우회 처리합니다.
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = -Character->GetActorForwardVector();
    }

    // 클라이언트와 서버간의 위치 동기화를 완벽하게 보장하는 등속도 직선 루트 모션을 주입합니다.
    UAbilityTask_ApplyRootMotionConstantForce* RootMotionTask = 
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this,
            FName("RedRollRootMotion"),
            RollDirection,
            RollSpeed,
            RollDuration,
            false,
            nullptr,
            ERootMotionFinishVelocityMode::SetVelocity,
            FVector::ZeroVector,
            0.0f,
            false
        );

    if (RootMotionTask)
    {
        // 구르기 제한 시간이 완결되면 안전 탈출 및 리소스 청소 콜백 배관을 연결합니다.
        RootMotionTask->OnFinish.AddDynamic(this, &UFT_RedRollSkill::OnRootMotionTimedOut);
        RootMotionTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
}

UGameplayEffect* UFT_RedRollSkill::GetCooldownGameplayEffect() const
{
    if (CooldownGameplayEffectClass)
    {
        return CooldownGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
    }

    return Super::GetCooldownGameplayEffect();
}

void UFT_RedRollSkill::OnRootMotionTimedOut()
{
    // 루트 모션 이동 시퀀스가 완결되면 어빌리티 공식 종료 관문으로 이동시킵니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_RedRollSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character && Character->GetCharacterMovement())
    {
        // 구르기 도중 하드 CC를 맞아 기술이 강제 캔슬당했다면 관성으로 인해 맵 밖으로 튕겨 나가는 현상을 막기 위해 캐릭터 물리 속도를 즉시 분쇄 동결합니다.
        if (bWasCancelled)
        {
            Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        }
    }

    // AOS 쿨다운 정책 완착: 구르기 모션 이동이 완전히 완료되어 마감된 바로 이 최종 시점에만 15초 고유 쿨타임을 발동시킵니다.
    // 만약 적의 하드 CC기에 의해 구르던 도중 강제 취소당한 상황이라면 쿨타임 패널티 없이 리턴 복귀합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}