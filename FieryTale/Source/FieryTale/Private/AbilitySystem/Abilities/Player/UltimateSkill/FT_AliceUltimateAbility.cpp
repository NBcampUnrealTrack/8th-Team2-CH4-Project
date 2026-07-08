// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AliceUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"

UFT_AliceUltimateAbility::UFT_AliceUltimateAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    // 시전 중임을 알리는 태그를 채널링 동안 소유하도록 락인합니다.
    ActivationOwnedTags.AddTag(FTTags::FTCombat::Skill_Channelling);
}

void UFT_AliceUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // [1단계] 궁극기 게이지 100% 전량 자원 소모 확정 관문
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // [2단계] 자신 중심 광역 시간 정지 원형 스캔 구역 개통
    FVector CenterLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    DrawDebugSphere(GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
#endif

    bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
        OverlapResults, CenterLocation, FQuat::Identity, ECollisionChannel::ECC_Pawn, ScanSphere, QueryParams);

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                // 💡 [피아식별 배관 완치]: 모호하던 비교 구문을 소각하고, 팀 진영 태그를 대조하여 아군 타격 프리패스 차단망을 적용합니다.
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // 적 영웅 진영일 경우에만 2초 기절(Stunned) 사양이 충전된 순정 디버프 GE를 다이렉트로 내리꽂습니다.
                if (TimeStopDebuffEffectClass)
                {
                    FGameplayEffectSpecHandle DebuffSpecHandle = MakeOutgoingGameplayEffectSpec(TimeStopDebuffEffectClass, GetAbilityLevel());
                    if (DebuffSpecHandle.IsValid())
                    {
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DebuffSpecHandle.Data.Get());
                    }
                }
            }
        }
    }

    // [3단계] 타이머 예약:  헤더 장부의 'ReleaseAlice' 수신 구역으로 1.0초 정밀 타이머 링크를 연동합니다.
    GetWorld()->GetTimerManager().SetTimer(
        AliceReleaseTimerHandle, 
        this, 
        &UFT_AliceUltimateAbility::ReleaseAlice, 
        1.0f, 
        false
    );
}

void UFT_AliceUltimateAbility::ReleaseAlice()
{
    // 명시적 콤보 종료 마감 관문으로 어빌리티 수명 주기를 완전하게 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 안전 해제: 1초 채널링 도중 적에게 죽거나 끊겼을 때 타이머 댕글링 누수 소각
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AliceReleaseTimerHandle);
        AliceReleaseTimerHandle.Invalidate();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}