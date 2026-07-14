// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_HitReactionAbility.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayTags/FTTags.h"

UFT_HitReactionAbility::UFT_HitReactionAbility()
{
    // 피격 애니메이션 제어를 위해 액터별 인스턴싱 정책을 적용합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 사망(Dead) 상태인 대상은 피격 반응 어빌리티를 활성화할 수 없도록 차단합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Core::Dead);
    
    // 어빌리티 에셋 태그 컨테이너를 초기화합니다.
    FGameplayTagContainer AssetTagContainer;
    SetAssetTags(AssetTagContainer);
}

void UFT_HitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 가능 여부를 검증합니다.
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

    APawn* AvatarPawn = Cast<APawn>(AvatarActor);
    if (AvatarPawn)
    {
        // AI 미니언일 경우 피격 시 즉시 이동을 중지합니다.
        if (AvatarPawn->IsPlayerControlled() == false)
        {
            if (AAIController* AIC = Cast<AAIController>(AvatarPawn->GetController()))
            {
                AIC->StopMovement();
            }
        }
    }

    // 피격 애니메이션 몽타주를 재생합니다.
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
        // 몽타주 종료 콜백을 바인딩합니다.
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
    // 피격 애니메이션이 완료되거나 중단되었으므로 어빌리티를 종료합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}