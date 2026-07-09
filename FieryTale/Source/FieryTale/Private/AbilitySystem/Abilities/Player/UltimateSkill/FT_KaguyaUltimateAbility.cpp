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

UFT_KaguyaUltimateAbility::UFT_KaguyaUltimateAbility()
{
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
    // =========================================================================
    // 💡 [1단계: 부모 가드선 최상단 전진 배치 - 버그 완전 박멸선]
    // Super를 최상단으로 복구하여 부모의 코스트 직접 소각 및 궁극기 판정 태그
    // 동 동기화 선로를 완벽하게 선제 고착합니다. 이로써 타격 시 100 리필 릭이 완전 소각됩니다.
    // =========================================================================
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [2단계] 자원 소모 및 선행 조건 검문
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

    // [3단계] 자신 중심 12미터 광역 오버랩 스캔 가동
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
        
        const bool bHasAuthority = IsPredictingClient() == false;

        if (bHasOverlap && bHasAuthority)
        {
            for (const FOverlapResult& Result : OverlapResults)
            {
                AActor* TargetActor = Result.GetActor();
                // 💡 [액세스 위반 크래시 완전 완치]: 포인터 생존 유효성을 1차 검문합니다.
                if (!IsValid(TargetActor)) continue;

                UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                if (TargetASC)
                {
                    // 피아 식별 필터 레이어
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;
                    
                    if (DamageGameplayEffectClass)
                    {
                        FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                        if (DamageSpecHandle.IsValid())
                        {
                            DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                            TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                        }
                    }

                    // 포탑 및 넥서스 등 구조체 면역 필터
                    bool bIsStructure = TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret) || 
                                        TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus);

                    // 💡 구조물이 아닌 일반 생명체(플레이어, 미니언) 캐릭터일 때만 CC 연산 파이프라인을 전개합니다.
                    if (!bIsStructure)
                    {
                        // 2) 중심부 강제 인장 (Pull Mechanism - 서버에서 다이렉트 연산 전파)
                        ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
                        if (IsValid(TargetCharacter))
                        {
                            FVector TargetLocation = TargetCharacter->GetActorLocation();
                            FVector PullDirection = (KaguyaLocation - TargetLocation);
                            PullDirection.Z = 0.f; // Z축 물리 왜곡 차단
                            
                            float Distance = PullDirection.Size();
                            
                            if (Distance > 15.0f)
                            {
                                PullDirection.Normalize();

                                float CalculatedForce = Distance * PullForceMultiplier;
                                CalculatedForce = FMath::Clamp(CalculatedForce, 300.0f, 2500.0f);

                                FVector LaunchVelocity = PullDirection * CalculatedForce;
                                LaunchVelocity.Z = 100.f; // 중앙 에어본 인장 유도

                                TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                            }
                        }

                        // 3) 2초간 80% 전술 둔화 디버프 주입
                        // 💡 [이동속도 릭 차단]: 구조체 면역 필터 내부로 정순 격리 결착하여 연산 오염을 차단합니다.
                        if (SlowDebuffEffectClass)
                        {
                            FGameplayEffectSpecHandle SlowSpecHandle = MakeOutgoingGameplayEffectSpec(SlowDebuffEffectClass, GetAbilityLevel());
                            if (SlowSpecHandle.IsValid())
                            {
                                TargetASC->ApplyGameplayEffectSpecToSelf(*SlowSpecHandle.Data.Get());
                            }
                        }
                    }
                }
            }
        }
    }

    // 단발성 즉시 격발 스킬이므로 시퀀스 무결 마감 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFT_KaguyaUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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