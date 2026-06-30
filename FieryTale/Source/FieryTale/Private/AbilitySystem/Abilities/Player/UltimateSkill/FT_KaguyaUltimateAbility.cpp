// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_KaguyaUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaUltimateAbility::UFT_KaguyaUltimateAbility()
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 궁극기 전용 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);
}

void UFT_KaguyaUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 자원 및 궁극기 게이지 비용을 차감합니다
    CommitAbilityCost(Handle, ActorInfo, ActivationInfo);
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 순정 로깅 인프라: Kismet 라이브러리를 걷어내고 데디케이트 서버 및 클라이언트 공용 로그 채널로 대체합니다
    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("Kaguya Ultimate Activated: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    // ---  1단계: 자신 중심 12m 광역 스캔 및 천인 강림  ---
    FVector KaguyaLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(AscensionRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    if (bDrawDebugs && GEngine)
    {
        // <--- 엔진 순정 디버그 출력 함수명으로 완벽 복구 완료!
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Kaguya Ultimate: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        KaguyaLocation,
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
                // [이중 방어선 1단계] 아군 오사 완전 필터링
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;
                
                // --- 📦 1. 확정 대미지 120 주입 (적 플레이어 + 미니언 + 구조물 공통) ---
                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }

                // --- [AOS 예외 처리 규격] 타겟이 구조물(포탑/넥서스)인지 검증 ---
                bool bIsStructure = TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret) || 
                                    TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus);

                // 타겟이 구조물이 아닐 때만(플레이어 및 미니언) 당겨오기 및 슬로우 적용
                if (!bIsStructure)
                {
                    // --- 2. 중심부 강제 인장 (Pull Mechanism) ---
                    ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
                    if (TargetCharacter)
                    {
                        FVector TargetLocation = TargetCharacter->GetActorLocation();
                        
                        // 가구야 공주를 향하는 방향 벡터 계산 (Z축 고정하여 바닥으로만 끌려오게 처리)
                        FVector PullDirection = (KaguyaLocation - TargetLocation);
                        PullDirection.Z = 0.f; 
                        
                        float Distance = PullDirection.Size();
                        PullDirection.Normalize();

                        // 거리에 비례하여 적절한 속도로 빨려 들어오도록 발사 벡터 구성
                        FVector LaunchVelocity = PullDirection * (Distance * PullForceMultiplier);
                        // 공중 에어본 방지 및 안전한 밀림을 위해 Z축 속도는 보정
                        LaunchVelocity.Z = 100.f; 

                        // 캐릭터 무브먼트를 통한 하드 인장 물리 격발
                        TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                    }

                    // --- 3. 2초간 80% 슬로우 디버프 주입 ---
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

    // 일시적인 광역격발 스킬이므로 즉시 안전하게 어빌리티를 정리하고 소멸 관문으로 진입합니다
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}