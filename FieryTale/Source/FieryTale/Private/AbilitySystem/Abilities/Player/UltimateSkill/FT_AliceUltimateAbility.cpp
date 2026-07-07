// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AliceUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UFT_AliceUltimateAbility::UFT_AliceUltimateAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    ActivationOwnedTags.AddTag(FTTags::FTCombat::Skill_Channelling);
}

void UFT_AliceUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
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

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("\"아직 6시야! 무대는 끝나지 않아!\""));
    }

    // 자신 중심 광역 시간 정지 원형 스캔 구역
    FVector CenterLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (bDrawDebugs)
    {
        DrawDebugSphere(OwnerActor->GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
    }
#endif

    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        CenterLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_Pawn,
        ScanSphere,
        QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

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

    // 안전한 오브젝트 포인터 콜백 타이머 구동 구역
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
}

void UFT_AliceUltimateAbility::ReleaseAlice()
{
    if (!CurrentActorInfo) return;

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("앨리스 행동 가능! 프리딜 타임 시작!"));
    }
    
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 예외 방어: 앨리스가 1초 타이머가 끝나기 전에 비정상 취소되더라도 가동 중이던 타이머 잔재 핸들을 완벽하게 청소 수거하고 무효화합니다.
    if (GetWorld() && AliceReleaseTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(AliceReleaseTimerHandle);
        AliceReleaseTimerHandle.Invalidate();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}