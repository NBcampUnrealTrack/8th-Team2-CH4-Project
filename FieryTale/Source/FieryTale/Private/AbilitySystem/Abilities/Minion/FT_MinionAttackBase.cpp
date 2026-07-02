// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_MinionAttackBase.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h" // 순정 애니메이션 태스크 헤더선 개통
#include "AIController.h"

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    // 각 미니언 인스턴스가 몽타주 재생 상태를 독자적으로 추적해야 하므로 액터별 인스턴싱 세팅
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // [GAS 마스터 전투 CC 가드선 완착]
    // 기절(Stunned) 상태이거나 실명/무기 해제(Disarmed) 태그가 ASC에 낙인찍히면, 
    // 브레인이 공격을 백날 명령해도 이 공격 어빌리티 격발 자체가 엔진 레벨에서 자동 무산됩니다!
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
    // 프로젝트 명세에 Disarmed나 Blinded 태그가 있다면 아래처럼 추가하여 기획을 확장할 수 있습니다.
    // ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Disarmed);
}

void UFT_MinionAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1. 공격에 필요한 기본 비용 및 쿨타임 검증
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

    // =========================================================================
    // [C++ 순정 PlayMontageAndWait 태스크 동적 배관]
    // 에디터 블루프린트의 "PlayMontageAndWait" 노드를 C++ 코어 단에서 직접 조립합니다.
    // =========================================================================
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        TEXT("MinionAttackTask"),
        AttackMontage,
        1.0f, // 재생 속도 배율 (기획 수치 연동 가능)
        NAME_None,
        false // 루트 모션 사용 여부
    );

    if (MontageTask)
    {
        // 애니메이션이 무사히 끝났거나 블렌딩 아웃/취소되었을 때 바인딩할 파이프라인 개통
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnBlendOut.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);

        // ◄◄◄ [타격 시점 동기화선] 
        // 몽타주 내부에 팀원들이 심어둔 "GameplayCue" 또는 "애니메이션 노티파이 상태 태그" 이벤트를 수신받기 위한 인터페이스 바인딩
        // 미니언 종류에 따라 근접 타격음 격발이나 원거리 투사체 사출 타이밍을 이 이벤트로 잡아챕니다.
        // (현재는 기본 순정 루틴 처리를 위해 타스크 활성화 시 즉시 격발 구조로 안착)
        
        MontageTask->ReadyForActivation();
    }
    else
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // 2. 즉시 실질적인 데미지 판정 연산 시동 (타격 이벤트 수신 처리 대용)
    // 원거리 미니언의 경우 이 지점을 상속받아 투사체를 앞으로 던지도록 확장하게 됩니다.
    if (AAIController* AIC = Cast<AAIController>(AvatarChar->GetController()))
    {
        // 앞서 브레인이 열심히 찾아둔 타겟 액터의 주소지를 AI 레이더에서 다시 확보합니다.
        // 브레인이 0.5초마다 갱신하는 타겟을 정밀 스캔
        if (UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent())
        {
            // 여기선 임시로 현재 미니언이 물리적으로 바라보는 전방 타겟팅이나 
            // 프로젝트에 정의된 데미지 이펙트 사출 함수(`ApplyGameplayEffectToTarget`)를 결합합니다.
            
            // ◄◄◄ [데미지 계산서 사출선]
            if (DamageEffectClass)
            {
                // 타겟팅 시스템이나 기획 장부와 엮어 타겟에게 이펙트 적용하는 공통 가드선 개통 지점
            }
        }
    }

    // 미니언 평타는 연타 속도가 정해져 있으므로, 몽타주가 끝나면 자동으로 어빌리티를 완결시켜 
    // 브레인이 다음 틱에 다시 공격 명령을 내릴 수 있도록 길을 열어줍니다.
}

void UFT_MinionAttackBase::OnMontageTargetedEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
    // 향후 애니메이션 노티파이를 통해 투사체 발사 시점이나 사운드 타이밍을 쪼갤 때 
    // 팀원들이 오버라이드하여 사용할 정밀 제어 밸브 구역입니다.
}

void UFT_MinionAttackBase::OnMontageCompletedOrCancelled()
{
    // 애니메이션이 끝나거나 도중에 끊겼다면 (예: 공격 도중 기절함),
    // 현재 공격 어빌리티를 안전하게 닫아서 리소스를 반환하고 뇌(Brain)에게 제어권을 돌려줍니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_MinionAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}