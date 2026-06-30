// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    // [FieryTale 표준 규격 통일] 
    // 궁극기 연산의 동기화 및 인스턴싱 정책을 다른 액티브 스킬들과 완벽히 일치시킵니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [에셋 태그 등록 표준화] 
    // FTTags 테이블을 토대로 '궁극기(UltimateSkill)' 정체성을 공식 주입합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);
}

void UFT_WolfRoarAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    CommitAbilityCost(Handle, ActorInfo, ActivationInfo);
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 순정 로깅 인프라: Kismet 라이브러리를 걷어내고 빌드 환경에 무해한 순정 로그 채널로 전면 대체합니다
    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("WolfRoar Activated: 할머니를 삼켰던 이빨로 적들을 물어뜯어라."));
    }

    // --- 전방 부채꼴 볼륨 확정 타격 물리 스캔 구역 ---
    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();

    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(HuntRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        StartLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_Pawn, 
        ScanSphere,
        QueryParams
    );

    if (bHasOverlap)
    {
        float DotThreshold = FMath::Cos(FMath::DegreesToRadians(ConeAngle * 0.5f));

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation).GetSafeNormal();
            float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

            // 부채꼴 각도 검증
            if (CurrentDot < DotThreshold) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();

            if (TargetASC)
            {
                // [이중 방어선 1단계] 물리 스캔 단에서 아군 오사 차단 (디버프 릭 원천 방쇄)
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // --- 150 고정 대미지 확정 주입 ---
                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, 150.f);
        
                        // 타깃 데이터 핸들 변환 필요 없이, 타깃의 ASC에 직접 스펙을 주입합니다
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }

                // --- 2초간 속박(이동 불가) 상태 이상 주입 ---
                if (RootGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle RootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                    if (RootSpecHandle.IsValid())
                    {
                        // 핸들 포인터 우회 없이 깔끔하게 직통 주입
                        TargetASC->ApplyGameplayEffectSpecToSelf(*RootSpecHandle.Data.Get());
                    }
                }
            }
        }
    }

    // 단발성 즉시 발동 기술이므로 처리가 끝나면 안전하게 정리 관문으로 연결합니다
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}