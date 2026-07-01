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
    // [인스턴싱 정책] 영웅 개별적으로 구르기 거리, 방향, 쿨다운 버퍼를 독립 추적하기 위해 액터당 인스턴스로 제어합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [네트워크 정책] 회피 이동기 특성상 렉 없이 즉시 반응하도록 클라이언트 선입력 예측 시스템을 기동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [인풋 태그 등록] 가상 함수 대신 최신 규격 규정에 맞춰 에셋 자체 식별용 태그를 장부에 낙인합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    // [쿨다운 태그 명세] 기술 격발 시 커밋 관문에서 이 자원 제어 태그를 참조하여 쿨다운 GE를 가동합니다.
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // [버프 실시간 연동] 구르기가 작동하는 동안 시전자의 몸에 Evading(회피/무적) 버프 태그를 자동 부착합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Evading);
}

void UFT_RedRollSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // [최선행 자원 검증] 글로벌 쿨다운 및 생존기 공용 쿨다운(15초) 장부를 검사하고 쿨다운 GE를 자동 사출합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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

    // [이동 방위 연산] 플레이어가 입력 중인 방향 패킷(Acceleration)을 분석하여 순정 물리 이동 방향을 확보합니다.
    FVector RollDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();

    // [예외 우회 가드] 만약 키보드 입력이 일절 없는 정지 상태라면, 영웅의 정면 기준 180도 반대인 '후방 구르기'로 우회 처리합니다.
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = -Character->GetActorForwardVector();
    }

    // [루트 모션 태스크 기동] 클라이언트와 서버간의 위치 동기화를 완벽하게 보장하는 등속도 직선 루트 모션을 주입합니다.
    UAbilityTask_ApplyRootMotionConstantForce* RootMotionTask = 
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this,
            FName("RedRollRootMotion"),
            RollDirection, // 구를 방향 벡터
            RollSpeed,     // 속도 수치
            RollDuration,  // 구르는 시간 (지속 시간)
            false,         // 낙하산/공중 중력 옵션 해제
            nullptr,       // 감속 커브 미사용 (등속 운동)
            ERootMotionFinishVelocityMode::SetVelocity, // 태스크 종료 후 속도 물리 처리 모드
            FVector::ZeroVector, // 종료 시 고정 잔여 속도
            0.0f,          // 추가 보정 속도 배율
            false          // 경로 강제 정렬 해제
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

// [조상 함수 정석 우회 반환] 블루프린트 에디터 디테일 창에 장착해 둔 CooldownGameplayEffectClass 클래스 원본 객체를
// GAS 프레임워크 코어 엔진으로 안전하게 상향 패스 연동합니다.
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
        // 만약 구르기 도중 하드 CC(기절 등)를 맞아 기술이 강제 캔슬당했다면,
        // 관성으로 인해 맵 밖으로 튕겨 나가는 현상을 막기 위해 캐릭터 물리 속도를 0으로 즉시 분쇄 동결합니다.
        if (bWasCancelled)
        {
            Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        }
    }

    // 최종 내부 상태 릴리즈 및 부모 가상 함수 호출
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}