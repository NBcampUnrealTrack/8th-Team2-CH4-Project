// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill); 
    SetAssetTags(AssetTags);

    HuntRadius = 800.0f;       
    ConeAngle = 90.0f;         
    BaseDamageValue = 150.0f;  
}

void UFT_WolfRoarAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // =========================================================================
    // 💡 [1단계: 부모 가드선 최상단 전진 배치 - 버그 완전 박멸선]
    // Super를 최상단으로 복구하여 부모의 코스트 직접 소각 및 궁극기 판정 태그
    // 동 동기화 선로를 완벽하게 선제 고착합니다. 이로써 타격 시 100 리필 릭이 완전 소각됩니다.
    // =========================================================================
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계] 마스터 베이스 게이지 소모 및 검증 관문 격발
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

    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("WolfRoar Activated: 할머니를 삼켰던 이빨로 적들을 물어뜯어라."));
    }

    // =========================================================================
    // 💡 [서버 주권 연산 레이어 완전 격리]
    // =========================================================================
    if (HasAuthority(&ActivationInfo))
    {
        FVector StartLocation = OwnerActor->GetActorLocation();
        FVector ForwardVector = OwnerActor->GetActorForwardVector();
        
        ForwardVector.Z = 0.f; // 고저차 방향 왜곡 원천 차단
        ForwardVector.Normalize();

        TArray<FOverlapResult> OverlapResults;
        FCollisionShape ScanSphere = FCollisionShape::MakeSphere(HuntRadius);
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerActor);

        if (GetWorld())
        {
            bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
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
                    // 💡 [액세스 위반 크래시 완전 완치]: 포인터 생존 유효성을 먼저 검문합니다.
                    if (!IsValid(TargetActor)) continue;

                    FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation);
                    TargetDirection.Z = 0.f; // 2층 지형 수평 보정
                    TargetDirection.Normalize();

                    float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

                    // 부채꼴 시야각 외각 대상 필터링 탈출
                    if (CurrentDot < DotThreshold) continue;

                    UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                    if (TargetASC)
                    {
                        // AOS 피아식별 안전망 가동 (팀킬 방지)
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                        // 1) 대미지 이펙트 사출
                        if (DamageGameplayEffectClass)
                        {
                            FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                            if (DamageSpecHandle.IsValid())
                            {
                                DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                                TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                            }
                        }

                        // 2) 군중 제어 속박(Root) 디버프 주입
                        if (RootGameplayEffectClass)
                        {
                            FGameplayEffectSpecHandle RootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                            if (RootSpecHandle.IsValid())
                            {
                                TargetASC->ApplyGameplayEffectSpecToSelf(*RootSpecHandle.Data.Get());
                            }
                        }
                    }
                }
            }
        }
    }
    
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFT_WolfRoarAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 💡 [자가 수급 전선 초기화 마감]:
    // 어빌리티 해제 파이프라인에서 부모가 동적으로 얹어두었던 느슨한 태그를 완전히 소각합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}