// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_KaguyaUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystemBlueprintLibrary.h"

UFT_KaguyaUltimateAbility::UFT_KaguyaUltimateAbility()
{
    // 본체 개체별 독립적인 상태 추적 및 판정을 위해 인스턴싱 정책을 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    AscensionRadius = 1200.0f; 
    BaseDamageValue = 120.0f;  
    PullForceMultiplier = 2.5f; 
}

void UFT_KaguyaUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // [1단계]: 부모 가드선 최상단 전진 배치
    // Super를 최상단으로 복구하여 부모의 코스트 직접 소각 및 궁극기 판정 태그 동기화 선로를 완벽하게 선제 고착합니다.
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계]: 자원 소모 및 선행 조건 검문을 통과시킵니다.
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
        UE_LOG(LogTemp, Log, TEXT("Kaguya Ultimate Activated: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Kaguya Ultimate: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    // [3단계]: 자신 중심의 광역 원형 오버랩 스캔을 가동합니다.
    FVector KaguyaLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(AscensionRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (bDrawDebugs && GetWorld())
    {
        DrawDebugSphere(GetWorld(), KaguyaLocation, AscensionRadius, 32, FColor::Orange, false, 2.f, 0, 2.f);
    }
#endif

    if (GetWorld())
    {
        bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
            OverlapResults, 
            KaguyaLocation, 
            FQuat::Identity, 
            ECollisionChannel::ECC_Pawn, 
            ScanSphere, 
            QueryParams
        );
        
        // =========================================================================
        // 💡 [치명적 2중 대미지/CC 및 중복 끌어당김 버그 완치 방어선]
        // 오직 서버 권한(HasAuthority) 오너쉽 권역에서만 대미지 적용, 군중 제어(CC) 인장, 
        // 물리 런치 연산이 유일하게 딱 1회 실행되도록 필터 가드를 완벽 배관합니다!
        // =========================================================================
        if (bHasOverlap && OwnerActor->HasAuthority())
        {
            // 💡 [최적화 마스터 레이어 개통]: 루프 바깥에서 대미지/감속 마스터 계산서 딱 1장씩만 선제 발행!
            FGameplayEffectSpecHandle MasterDamageSpecHandle;
            FGameplayEffectSpecHandle MasterSlowSpecHandle;

            // 1. 대미지 마스터 장부 선제 인스턴스화
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

            // 2. 이속 감소 마스터 장부 선제 인스턴스화
            if (SlowDebuffEffectClass)
            {
                MasterSlowSpecHandle = MakeOutgoingGameplayEffectSpec(SlowDebuffEffectClass, GetAbilityLevel());
                if (MasterSlowSpecHandle.IsValid() && MasterSlowSpecHandle.Data.IsValid())
                {
                    MasterSlowSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                    MasterSlowSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                }
            }

            // -------------------------------------------------------------------------
            // 💡 마스터 장부 무장 완료 후 오버랩 대상 타깃 고속 정산 루프 기동
            // -------------------------------------------------------------------------
            for (const FOverlapResult& Result : OverlapResults)
            {
                AActor* HitActor = Result.GetActor();
                if (!IsValid(HitActor)) continue;

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
                if (TargetASC)
                {
                    // 피아 식별 필터 레이어를 가동하여 아군 타격 피해를 완벽히 격리 차단합니다.
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;
                    
                    // 이미 사망 상태인 타깃 시체 구타 차단
                    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

                    // 공통 피격 히트 결과 장부 스냅샷 생성
                    FHitResult IndividualHit;
                    IndividualHit.HitObjectHandle = HitActor;
                    IndividualHit.Location = HitActor->GetActorLocation();
                    IndividualHit.ImpactPoint = HitActor->GetActorLocation();

                    // 1) 💡 [초고속 장부 돌려막기 1]: 힙 할당 없이 복사본만 찍어 대미지 주입
                    if (MasterDamageSpecHandle.IsValid() && MasterDamageSpecHandle.Data.IsValid())
                    {
                        FGameplayEffectSpec LocalDamageSpec(*MasterDamageSpecHandle.Data.Get());
                        FGameplayEffectContextHandle LocalContext = LocalDamageSpec.GetContext().Duplicate();
                        LocalContext.AddHitResult(IndividualHit, true);
                        LocalDamageSpec.SetContext(LocalContext);

                        SourceASC->ApplyGameplayEffectSpecToTarget(LocalDamageSpec, TargetASC);
                    }

                    // 포탑 및 넥서스 등 고정형 월드 구조체 면역 필터를 타설합니다.
                    bool bIsStructure = TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret) || 
                                        TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus);

                    // 구조물이 아닌 일반 생명체(플레이어, 미니언) 캐릭터 레이어일 때만 강력한 강제 인장 및 디버프 연산을 가동합니다.
                    if (!bIsStructure)
                    {
                        // 2) 중심부 강제 인장 물리 제어 (Pull Mechanism)
                        ACharacter* TargetCharacter = Cast<ACharacter>(HitActor);
                        if (IsValid(TargetCharacter))
                        {
                            FVector TargetLocation = TargetCharacter->GetActorLocation();
                            FVector PullDirection = (KaguyaLocation - TargetLocation);
                            PullDirection.Z = 0.f; // Z축 변위 고정으로 물리 연산 왜곡 파괴를 차단합니다.
                            
                            float Distance = PullDirection.Size();
                            
                            if (Distance > 15.0f)
                            {
                                PullDirection.Normalize();

                                float CalculatedForce = Distance * PullForceMultiplier;
                                CalculatedForce = FMath::Clamp(CalculatedForce, 300.0f, 2500.0f);

                                FVector LaunchVelocity = PullDirection * CalculatedForce;
                                LaunchVelocity.Z = 100.f; // 중앙 집결부 에어본 인장을 정밀하게 유도합니다.

                                TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                            }
                        }

                        // 3) 💡 [초고속 장부 돌려막기 2]: 감속 디버프도 스택 복사본으로 안전 주입
                        if (MasterSlowSpecHandle.IsValid() && MasterSlowSpecHandle.Data.IsValid())
                        {
                            FGameplayEffectSpec LocalSlowSpec(*MasterSlowSpecHandle.Data.Get());
                            FGameplayEffectContextHandle LocalContext = LocalSlowSpec.GetContext().Duplicate();
                            LocalContext.AddHitResult(IndividualHit, true);
                            LocalSlowSpec.SetContext(LocalContext);

                            SourceASC->ApplyGameplayEffectSpecToTarget(LocalSlowSpec, TargetASC);
                        }
                    }
                }
            }
        }
    }

    // 단발성 즉시 격발 기술이므로 연산 직후 시퀀스를 클린 마감 종료 처리합니다.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFT_KaguyaUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 자가 수급 전선 초기화 마감: 어빌리티 수명 해제 파이프라인에서 시전 중 장착해 두었던 느슨한 태그를 완벽히 회수 철거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}