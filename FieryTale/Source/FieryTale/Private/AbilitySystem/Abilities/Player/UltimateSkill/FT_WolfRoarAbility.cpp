// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystemBlueprintLibrary.h"

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

                // =========================================================================
                // 💡 [초고속 광역기 메모리 최적화 레이어 개통]
                // 루프 바깥에서 공용 대미지/속박(Root) 계산서 장부를 딱 1장씩만 힙 할당하여 고정합니다.
                // 이로써 타깃이 수십 마리 겹쳐도 메모리 파편화 및 성능 드롭을 원천 차단합니다.
                // =========================================================================
                FGameplayEffectSpecHandle MasterDamageSpecHandle;
                FGameplayEffectSpecHandle MasterRootSpecHandle;

                // 1) 대미지 마스터 장부 선제 인스턴스화
                if (DamageGameplayEffectClass)
                {
                    MasterDamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (MasterDamageSpecHandle.IsValid() && MasterDamageSpecHandle.Data.IsValid())
                    {
                        MasterDamageSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                        MasterDamageSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                        MasterDamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                    }
                }

                // 2) 군중 제어 속박(Root) 마스터 장부 선제 인스턴스화
                if (RootGameplayEffectClass)
                {
                    MasterRootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                    if (MasterRootSpecHandle.IsValid() && MasterRootSpecHandle.Data.IsValid())
                    {
                        MasterRootSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                        MasterRootSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                    }
                }

                // -------------------------------------------------------------------------
                // 💡 마스터 장부 장착 후 부채꼴 타격 검출 루프 고속 기동
                // -------------------------------------------------------------------------
                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* TargetActor = Result.GetActor();
                    if (!IsValid(TargetActor)) continue;

                    FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation);
                    TargetDirection.Z = 0.f; // 수평 조준 보정
                    TargetDirection.Normalize();

                    float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

                    // 부채꼴 외각 대상 즉각 제외
                    if (CurrentDot < DotThreshold) continue;

                    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
                    if (TargetASC)
                    {
                        // AOS 피아식별 안전망 (아군 오사 대미지/CC 면제)
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                        // 타깃 사망 시 시체 구타 방지 검문
                        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

                        // 타깃 적중 히트 데이터 생성 (공통 스냅샷)
                        FHitResult IndividualHit;
                        IndividualHit.HitObjectHandle = TargetActor;
                        IndividualHit.Location = TargetActor->GetActorLocation();
                        IndividualHit.ImpactPoint = TargetActor->GetActorLocation();

                        // 💡 [스택 사본 복사 돌려막기 1 - 대미지]
                        if (MasterDamageSpecHandle.IsValid() && MasterDamageSpecHandle.Data.IsValid())
                        {
                            FGameplayEffectSpec LocalDamageSpec(*MasterDamageSpecHandle.Data.Get());
                            FGameplayEffectContextHandle LocalContext = LocalDamageSpec.GetContext().Duplicate();
                            LocalContext.AddHitResult(IndividualHit, true);
                            LocalDamageSpec.SetContext(LocalContext);

                            SourceASC->ApplyGameplayEffectSpecToTarget(LocalDamageSpec, TargetASC);
                        }

                        // 💡 [스택 사본 복사 돌려막기 2 - 속박 디버프]
                        if (MasterRootSpecHandle.IsValid() && MasterRootSpecHandle.Data.IsValid())
                        {
                            FGameplayEffectSpec LocalRootSpec(*MasterRootSpecHandle.Data.Get());
                            FGameplayEffectContextHandle LocalContext = LocalRootSpec.GetContext().Duplicate();
                            LocalContext.AddHitResult(IndividualHit, true);
                            LocalRootSpec.SetContext(LocalContext);

                            SourceASC->ApplyGameplayEffectSpecToTarget(LocalRootSpec, TargetASC);
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
    // 자가 수급 전선 초기화 마감: 필살기 식별 루즈 태그 안전 회수
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}