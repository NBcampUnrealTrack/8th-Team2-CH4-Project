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
    // =========================================================================
    // 💡 [1단계: 부모 가드선 선제 타설 - 버그 박멸 최종선]
    // Super를 최상단으로 전진 배치하여 부모의 코스트 강제 차감 파이프라인과
    // 궁극기 대미지 판정 태그(UltimateSkill)가 시전자 장부에 100% 선제 동기화되도록 보장합니다.
    // 이로써 타격 시 게이지가 다시 100으로 튀는 누수가 원천 차단됩니다.
    // =========================================================================
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계] 궁극기 작동 가능성 확인
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!IsValid(OwnerActor) || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // [3단계] 자신 중심 광역 시간 정지 원형 스캔 구역 개통
    FVector CenterLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (GetWorld())
    {
        DrawDebugSphere(GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
    }
#endif

    if (GetWorld())
    {
        bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
            OverlapResults, CenterLocation, FQuat::Identity, ECollisionChannel::ECC_Pawn, ScanSphere, QueryParams);

        if (bHasOverlap)
        {
            for (const FOverlapResult& Result : OverlapResults)
            {
                AActor* TargetActor = Result.GetActor();
                // 💡 [액세스 위반 크래시 완전 완치]: 포인터 생존 유효성을 엄격하게 사전 검문합니다.
                if (!IsValid(TargetActor)) continue;

                UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                if (TargetASC)
                {
                    // [피아식별 배관 완치]: 팀 진영 태그를 대조하여 아군 타격 프리패스 차단망을 적용합니다.
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                    // 적 영웅/미니언 진영일 경우에만 2초 기절(Stunned) 사양이 충전된 순정 디버프 GE를 다이렉트로 내리꽂습니다.
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

        // [4단계] 타이머 예약: 헤더 장부의 'ReleaseAlice' 수신 구역으로 1.0초 정밀 타이머 링크를 연동합니다.
        GetWorld()->GetTimerManager().SetTimer(
            AliceReleaseTimerHandle, 
            this, 
            &UFT_AliceUltimateAbility::ReleaseAlice, 
            1.0f, 
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AliceUltimateAbility::ReleaseAlice()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AliceReleaseTimerHandle);
        AliceReleaseTimerHandle.Invalidate();
    }

    // 💡 [글로벌 궁극기 자원 전선 초기화]:
    // 어빌리티 수명이 마감되는 시점에 부모 클래스가 부여해 두었던 궁극기 식별 느슨한 태그를 
    // 깔끔하게 철거 수거하여 다음 컴뱃 자원 사이클이 오염되지 않도록 멸균 마감합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}