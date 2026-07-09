// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_HitReactionAbility.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTags/FTTags.h"

UFT_HitReactionAbility::UFT_HitReactionAbility()
{
    // 본체 개체별 독립적인 피격 애니메이션 제어 및 상태 추적을 위해 액터별 인스턴싱 정책 적용
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 사망 상태(Dead)인 대상에게 피격 반응 경직이 중복 격발되는 현상을 엔진 레벨에서 원천 차단합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Core::Dead);
    
    // 신형 GAS API 규격에 맞춰 어빌리티 에셋 태그 컨테이너를 청정하게 초기화합니다.
    FGameplayTagContainer AssetTagContainer;
    SetAssetTags(AssetTagContainer);
}

void UFT_HitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 가능 여부 및 코스트 검증 관문을 수행합니다.
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

    // [유저 조작 영웅 vs AI 미니언 피격 제어 분기선]
    APawn* AvatarPawn = Cast<APawn>(AvatarActor);
    if (AvatarPawn)
    {
        // 몬스터 혹은 AI 미니언 개체일 경우 피격 경직 즉시 현재 기동 경로 및 이동 연산을 원자적으로 강제 중지합니다.
        if (AvatarPawn->IsPlayerControlled() == false)
        {
            if (AAIController* AIC = Cast<AAIController>(AvatarPawn->GetController()))
            {
                AIC->StopMovement();
            }
        }
    }

    // 피격 애니메이션 몽타주 재생 및 종료 시점 추적을 위한 순정 GAS 어빌리티 태스크를 시동합니다.
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
        // 델리게이트 바인딩 완치 완착: 저수준 매크로 파이프라인을 배제하고 언리얼 엔진 순정 공식 리플렉션 명세인 AddDynamic으로 결합을 수행합니다.
        HitMontageTask->OnCompleted.AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted);
        HitMontageTask->OnBlendOut.AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted);
        HitMontageTask->OnInterrupted.AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted);
        HitMontageTask->OnCancelled.AddDynamic(this, &UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted);
        
        HitMontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UFT_HitReactionAbility::OnHitMontageCompletedOrInterrupted()
{
    // 경직 애니메이션 처리가 완전히 완료되었으므로 어빌리티를 정리하고 안전 종료 분기로 진입합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}