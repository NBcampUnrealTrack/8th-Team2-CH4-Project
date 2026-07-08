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
    // [1단계] 자원 소모 및 선행 조건 검문
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

    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("Kaguya Ultimate Activated: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Kaguya Ultimate: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    // [2단계] 자신 중심 12미터 광역 오버랩 스캔 가동
    FVector KaguyaLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(AscensionRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (bDrawDebugs)
    {
        DrawDebugSphere(GetWorld(), KaguyaLocation, AscensionRadius, 32, FColor::Orange, false, 2.f, 0, 2.f);
    }
#endif

    // 💡 CenterLocation을 KaguyaLocation으로 정밀 교정 완착
    bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
        OverlapResults, 
        KaguyaLocation, 
        FQuat::Identity, 
        ECollisionChannel::ECC_Pawn, 
        ScanSphere, 
        QueryParams
    );
    // 💡 [서버 권한 검문선 락인]: 위치 왜곡 렉을 차단하기 위해 실질적인 스탯 변조와 물리 연산은 서버에서만 전개합니다.
    const bool bHasAuthority = IsPredictingClient() == false;

    if (bHasOverlap && bHasAuthority)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                // 피아 식별 필터 레이어
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;
                
                // 1) 광역 피해량 주입 파이프라인
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

                if (!bIsStructure)
                {
                    // 2) 중심부 강제 인장 (Pull Mechanism - 서버에서 다이렉트 연산 전파)
                    ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
                    if (TargetCharacter)
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

                            // 서버 권한에서 사출하여 클라이언트로 무브먼트 복제 패킷 강제 전파
                            TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                        }
                    }

                    // 3) 2초간 80% 전술 둔화 디버프 주입
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

    // 단발성 즉시 격발 스킬이므로 시퀀스 무결 마감 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}