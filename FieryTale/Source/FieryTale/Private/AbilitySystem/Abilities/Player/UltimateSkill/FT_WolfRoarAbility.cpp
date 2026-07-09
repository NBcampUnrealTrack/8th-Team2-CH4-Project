// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    // 본체 개체별 독립적인 상태 추적 및 판정을 위해 인스턴싱 정책을 확정합니다.
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
    // [1단계]: 부모 가드선 최상단 전진 배치
    // Super를 최상단으로 전진 배치하여 부모의 코스트 직접 소각 및 궁극기 판정 태그 동기화 선로를 완벽하게 선제 고착합니다.
    // 이로써 타격 정산 시 게이지가 100으로 되튀는 자원 복구 릭이 원천 박멸됩니다.
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계]: 마스터 베이스 게이지 소모 및 검증 관문을 격발합니다.
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

    // [3단계]: 서버 주권 연산 레이어 완전 격리
    // 클라이언트 예측 영역과의 데이터 변위 및 넉백, 상태이상 타격 싱크 뒤틀림을 원천 방어하기 위해 권한 오너쉽 검문을 가동합니다.
    if (HasAuthority(&ActivationInfo))
    {
        FVector StartLocation = OwnerActor->GetActorLocation();
        FVector ForwardVector = OwnerActor->GetActorForwardVector();
        
        ForwardVector.Z = 0.f; // 고저차 지형에 의한 전방 방향 벡터 왜곡을 원천 차단합니다.
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
                // 부채꼴 시야 내각 필터링을 위한 코사인 임계값(DotThreshold)을 선제 도출합니다.
                float DotThreshold = FMath::Cos(FMath::DegreesToRadians(ConeAngle * 0.5f));

                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* TargetActor = Result.GetActor();
                    // 액세스 위반 크래시 완전 완치: 포인터 생존 유효성을 촘촘하게 사전 검문합니다.
                    if (!IsValid(TargetActor)) continue;

                    FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation);
                    TargetDirection.Z = 0.f; // 2층 링크 복층 지형을 평면 벡터로 수평 조준 보정합니다.
                    TargetDirection.Normalize();

                    float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

                    // 내적 비교 계산을 수행하여 부채꼴 조준 각도 외각에 위치한 대상은 연산에서 즉각 제외시킵니다.
                    if (CurrentDot < DotThreshold) continue;

                    UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                    if (TargetASC)
                    {
                        // AOS 피아식별 안전망 가동: 동일 진영 태그 검증을 통해 아군에 대한 팀킬 피해 연산을 원천 차단합니다.
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                        // 1) 기반 대미지 이펙트 사출 배관 격발
                        if (DamageGameplayEffectClass)
                        {
                            FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                            if (DamageSpecHandle.IsValid())
                            {
                                DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                                TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                            }
                        }

                        // 2) 군중 제어 속박(Rooted) 상태이상 디버프 주입선 연결
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
    
    // 단발성 즉시 광역 포효 스킬이므로 연산 후 기동 시퀀스를 원자적으로 클린 마감 종료합니다.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFT_WolfRoarAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 자가 수급 전선 초기화 마감: 어빌리티 해제 파이프라인에서 부모 프레임워크가 얹어두었던 필살기 식별 루즈 태그를 안전하게 회수 소각합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}