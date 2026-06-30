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

    // [시전자 락] 앨리스가 시전하는 '1초 동안'은 본인도 채널링 상태(행동 불가)임을 태그로 명시합니다.
    // 이 태그는 어빌리티가 활성화되어 있는 동안 자동으로 시전자에게 부여됩니다.
    ActivationOwnedTags.AddTag(FTTags::FTCombat::Skill_Channelling);
}

void UFT_AliceUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 코스트 커밋 및 부모 가동 (게이지 리셋)
    CommitAbilityCost(Handle, ActorInfo, ActivationInfo);
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("\"아직 6시야! 무대는 끝나지 않아!\""));
    }

    // --- 🕒 1단계: 자신 중심 광역 시간 정지 (원형 스캔) 🕒 ---
    FVector CenterLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    if (bDrawDebugs)
    {
        DrawDebugSphere(OwnerActor->GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
    }

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
                // [이중 방어선 1단계] 아군 오사 완전 차단
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // 적들에게 2초 기절(Stunned 태그가 포함된 GE) 주입
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

    // --- ⏳ 2단계: 앨리스 시간차 행동 제어 타이머 구동 ⏳ ---
    // 1초 뒤에 앨리스를 채널링 락에서 풀어주는 타이머를 세팅합니다.
    TWeakObjectPtr<UFT_AliceUltimateAbility> WeakSelf(this);
    OwnerActor->GetWorld()->GetTimerManager().SetTimer(
        AliceReleaseTimerHandle,
        [WeakSelf, Handle, ActorInfo, ActivationInfo]()
        {
            if (UFT_AliceUltimateAbility* StrongSelf = WeakSelf.Get())
            {
                StrongSelf->ReleaseAlice(Handle, ActorInfo, ActivationInfo);
            }
        },
        1.0f, // 1초 뒤 가동
        false
    );
}

void UFT_AliceUltimateAbility::ReleaseAlice(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo)
{
    // 앨리스가 1초 뒤 먼저 자유 몸이 되는 순간!
    // ActivationOwnedTags에 엮여있던 Skill_Channelling 태그를 걷어내기 위해 어빌리티를 정상 종료 처리합니다.
    // 적들은 이미 각자 2초짜리 기절 GE를 받아 갔으므로, 앨리스가 먼저 풀려나도 적들은 앞으로 1초 더 멈춰있게 됩니다.
    
    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("앨리스 행동 가능! 프리딜 타임 시작!"));
    }
    
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}