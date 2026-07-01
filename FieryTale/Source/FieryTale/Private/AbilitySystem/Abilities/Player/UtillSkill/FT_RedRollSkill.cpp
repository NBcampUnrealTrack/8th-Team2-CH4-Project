// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_RedRollSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameplayTags/FTTags.h"

UFT_RedRollSkill::UFT_RedRollSkill()
    : RollSpeed(1500.0f)     // 기획 데이터 락인: 구르기 시 순간적으로 돌파할 루트 모션의 이동 속도 수치 명세
    , RollDuration(0.35f)    // 기획 데이터 락인: 구르기 모션 및 물리 가속이 유지될 한계 시간 0.35초 명세
{
    // 인스턴싱 정책: 영웅 캐릭터마다 독립적인 루트 모션 태스크 수명 주기와 상태를 통제하기 위해 인스턴스화 모드로 제어합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // NETWORK 실행 정책: 입력 즉시 지연 없이 구르기 반응성을 확보하기 위해 클라이언트 선입력 예측 시스템을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 시프트 유틸기 범주 식별을 위해 총괄님 수정 규격안대로 공용 UtilSkill 에셋 태그를 명확히 바인딩합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 빨간 망토 전용 시프트 생존 기술의 15초 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 시프트 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 하나로 결합됩니다.
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 시스템 자동 부여 태그 슬롯에 구르는 도중 일시적으로 회피 기동 상태 버프임을 마킹하는 상수를 완착합니다.
    // 런타임 흐름 요약: 이 능력이 유지되는 0.35초 동안 시전자 몸에 Evading 태그가 부착되며, 피해 연산기에서 무적 및 판정 흘리기 로직과 공명합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Evading);
}

void UFT_RedRollSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // GAS 마스터 규격: 시프트 스킬 쿨다운 상태를 선제 검사하고 자원 비용을 서버와 클라이언트에서 동시 승인 소소모합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        // 쿨타임 중이거나 기절 상태라면 즉시 어빌리티 수명주기를 안전 종료하여 리소스 누수를 차단합니다.
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

    // 플레이어의 현재 입력 키 패드 벡터를 실시간 스캔하여 실질적인 가속 이동 방향 벡터를 도출합니다.
    FVector RollDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();

    // 입력 예외 보정선: 만약 이동 입력을 전혀 주지 않은 정지 상태에서 생존기를 긴급 격발했다면,
    // 캐릭터가 바라보는 정면의 반대 벡터 방향인 후방으로 즉시 긴급 후퇴하도록 자동 안전 분기 처리합니다.
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = -Character->GetActorForwardVector();
    }

    // 완벽한 서버 클라이언트 동기화 및 가속 처리를 위해 GAS 순정 루트 모션 상수의 가속 힘 태스크 파이프라인을 개통합니다.
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
        // 지정된 루트 모션 가속 타임아웃 시간이 정상적으로 만료 완료되었을 때 OnFinish 콜백 경로를 연결합니다.
        RootMotionTask->OnFinish.AddDynamic(this, &UFT_RedRollSkill::OnRootMotionTimedOut);
        RootMotionTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
}

void UFT_RedRollSkill::OnRootMotionTimedOut()
{
    // 0.35초 루트 모션 이동 연산이 안전하게 완결되는 즉시 능력을 정리하는 마감 관문으로 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_RedRollSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 물리 리셋 예외 방어선 가동: 구르는 도중에 적의 강한 하드 CC기를 맞아 능력이 비정상 캔슬 하차당하더라도
    // 루트 모션 잔재 가속력 때문에 캐릭터가 계속 맵 끝까지 미끄러져 추락하는 물리 릭을 완전히 차단하기 위해 무브먼트의 가속 속도 벡터를 그 즉시 제로 벡터로 깔끔하게 클리어 수거합니다.
    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character && Character->GetCharacterMovement())
    {
        if (bWasCancelled)
        {
            Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        }
    }

    // 부모 가상 함수를 최종적으로 호출하는 순간 생성자 구역에서 마킹했던 Evading 회피 버프 태그가 자동으로 완벽하게 소멸 수거됩니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}