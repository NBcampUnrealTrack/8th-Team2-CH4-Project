// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_HitReactionAbility.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTags/FTTags.h"

UFT_HitReactionAbility::UFT_HitReactionAbility()
{
    // 본체 개체별 독립적인 피격 애니메이션 제어 및 상태 추적을 위해 액터별 인스턴싱 정책 적용
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 사망 상태(Dead)인 대상에게 피격 반응 경직이 중복 격발되는 현상을 엔진 레벨에서 원천 차단
    ActivationBlockedTags.AddTag(FTTags::FTStates::Core::Dead);
    
    // 신형 GAS API 규격에 맞춰 어빌리티 에셋 태그 컨테이너를 청정하게 초기화
    FGameplayTagContainer AssetTagContainer;
    SetAssetTags(AssetTagContainer);
}

void UFT_HitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 가능 여부 및 자원 검증 관문
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor || !HitReactionMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // =========================================================================
    // [유저 조작 영웅 vs AI 미니언 피격 제어 분기선]
    // 플레이어가 직접 제어하는 영웅 챔피언은 조작감 유지를 위해 제동을 걸지 않으며,
    // AI 전술 하에 움직이는 일반 미니언 개체일 때만 순간 제동(StopMovement)을 집도합니다.
    // =========================================================================
    APawn* AvatarPawn = Cast<APawn>(AvatarActor);
    if (AvatarPawn)
    {
        if (AvatarPawn->IsPlayerControlled() == false)
        {
            if (AAIController* AIC = Cast<AAIController>(AvatarPawn->GetController()))
            {
                AIC->StopMovement();
            }
        }
    }

    // 피격 애니메이션 몽타주 재생 및 종료 시점 추적을 위한 순정 GAS 어빌리티 태스크 시동
    UAbilityTask_PlayMontageAndWait* HitMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        TEXT("HitReactionTask"),
        HitReactionMontage,
        1.0f,
        NAME_None,
        false
    );

    if (HitMontageTask)
    {
        // 컴파일러 환경에 따른 매크로 템플릿 치환 오류를 방지하기 위해 리플렉션 시스템 전용 동적 바인딩 함수 활용
        HitMontageTask->OnCompleted.__Internal_AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted, FName(TEXT("OnHitMontageCompletedOrInterrupted")));
        HitMontageTask->OnBlendOut.__Internal_AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted, FName(TEXT("OnHitMontageCompletedOrInterrupted")));
        HitMontageTask->OnInterrupted.__Internal_AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted, FName(TEXT("OnHitMontageCompletedOrInterrupted")));
        HitMontageTask->OnCancelled.__Internal_AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted, FName(TEXT("OnHitMontageCompletedOrInterrupted")));
        
        HitMontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted()
{
    // 경직 애니메이션 처리가 완전히 완료되었으므로 어빌리티를 정리하고 닫음
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}