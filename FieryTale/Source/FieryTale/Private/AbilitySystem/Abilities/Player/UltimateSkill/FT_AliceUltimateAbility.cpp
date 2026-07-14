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
    // 본체 개체별 독립적인 상태 추적 및 타이머 제어를 위해 인스턴싱 정책을 확정합니다.
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
    // [1단계]: 부모 가드선 선제 타설
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계]: 궁극기 작동 가능성 확인
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

    // =========================================================================
    // 💡 [3단계]: 광역 시간 정지 원형 스캔 구역 개통 (2중 스턴 버그 멸균선 완착!)
    // 오직 서버(HasAuthority) 주권 하에서만 스캔을 돌리고 대상의 장부에 디버프를 인가합니다.
    // 클라이언트는 이 구역을 프리패스하여 중복 스턴 패킷이 발생하지 않습니다.
    // =========================================================================
    if (OwnerActor->HasAuthority())
    {
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
                    // 액세스 위반 크래시 완전 완치: 포인터 생존 유효성을 엄격하게 사전 검문합니다.
                    if (!IsValid(TargetActor)) continue;

                    UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                    if (TargetASC)
                    {
                        // 피아식별 배관 완치: 팀 진영 태그를 대조하여 아군 타격 프리패스 차단망을 적용합니다.
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                        // 적 영웅/미니언 진영일 경우에만 시간 정지(Stunned) 사양이 충전된 순정 디버프 GE를 다이렉트로 내리꽂습니다.
                        if (TimeStopDebuffEffectClass)
                        {
                            FGameplayEffectSpecHandle DebuffSpecHandle = MakeOutgoingGameplayEffectSpec(TimeStopDebuffEffectClass, GetAbilityLevel());
                            if (DebuffSpecHandle.IsValid())
                            {
                                // 💡 확실한 타깃팅을 위해 시전자(Instigator) 정보를 컨텍스트에 박아 넣고 직통 밸브로 꽂습니다.
                                DebuffSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                                DebuffSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                                
                                SourceASC->ApplyGameplayEffectSpecToTarget(*DebuffSpecHandle.Data.Get(), TargetASC);
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // [4단계]: 타이머 예약 (수명주기 동기화)
    // 서버와 클라이언트 양쪽에서 1.0초 타이머가 돌아야 시전자의 채널링과 이펙트가 정상적으로 풀립니다.
    // 따라서 이 구역은 HasAuthority 밖에서 정상 실행되도록 배치했습니다.
    // =========================================================================
    if (GetWorld())
    {
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

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 글로벌 궁극기 자원 전선 초기화: 어빌리티 수명이 마감되는 시점에 궁극기 식별 느슨한 태그를 깔끔하게 수거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);

        // 네트워크 동기화 쿨타임 환불: CC기나 상태이상으로 중간에 강제 캔슬된 경우 쿨타임 쿼리 수선선 적용
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            // 엔진 순정 표준 쿼리 배관인 MakeQuery_MatchAnyOwningTags로 교체하여 피격 캔슬 시 궁극기 자원 및 게이지 먹통 결함을 원천 진압합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}