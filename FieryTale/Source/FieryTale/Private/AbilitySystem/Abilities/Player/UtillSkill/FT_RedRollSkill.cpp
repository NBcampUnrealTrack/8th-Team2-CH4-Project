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
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Shift 유틸기 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // Shift 이동/생존 기술 공용 재사용 대기시간 태그 매핑 (데이터 에셋 설정상 빨간 망토는 15초 쿨타임 작동)
    CooldownTag = FTTags::FTStates::Cooldown_UtilSkill;

    // 구르는 동안 일시적으로 회피 기동 상태임을 나타내는 태그를 캐릭터에게 부여합니다
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.Evading")));
}

void UFT_RedRollSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 스킬을 쓸 수 있는 자원 상태(15초 쿨다운)를 검사하고 승인 처리합니다
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 플레이어의 현재 입력 키 벡터를 계산하여 이동 방향을 도출합니다
    FVector RollDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();

    // 만약 이동 입력이 전혀 없는 상태라면 캐릭터가 바라보는 정면의 반대 방향(후방)으로 후퇴하도록 강제합니다
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = -Character->GetActorForwardVector();
    }

    // 완벽한 서버 클라이언트 동기화 및 강제 이동 처리를 위해 GAS 순정 루트 모션 태스크를 가동합니다
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
        // 지정된 루트 모션 시간이 만료되어 완료되었을 때 OnFinish 델리게이트를 통해 콜백을 격발합니다
        RootMotionTask->OnFinish.AddDynamic(this, &UFT_RedRollSkill::OnRootMotionTimedOut);
        RootMotionTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 추후 구현 포인트 구르기 진입 시 재생할 애니메이션 몽타주 및 흙먼지 나이아가라 이펙트 연동 구역
}

void UFT_RedRollSkill::OnRootMotionTimedOut()
{
    // 루트 모션 이동이 끝나는 즉시 능력을 정리하는 관문으로 진입합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_RedRollSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 취소되지 않고 정상 종료될 때만 시스템 공용 쿨타임(15초)을 확정 가동시킵니다
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}