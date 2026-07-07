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
    // 인스턴싱 정책: 영웅별 독립적인 궁극기 사양 데이터를 유지 관리하기 위해 인스턴스화 모드로 가동합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬 뷰에서 광역 천인 강림 이펙트를 격발하여 반응성을 최적화합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 바인딩 가이드: 이 기술은 궁극기 범주로 식별되어야 하므로 전용 태그를 식별자에 주입합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    // 기획 데이터 락인: 스킬 데이터 에셋의 기본 설정값을 정밀 제어합니다.
    AscensionRadius = 1200.0f; // 12미터 광역 반경
    BaseDamageValue = 120.0f;  // 적중 시 주입할 기본 피해량
    PullForceMultiplier = 2.5f; // 중심부로 당기는 물리력 가중치
}

void UFT_KaguyaUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // GAS 마스터 규격: 궁극기 게이지 소모 파이프라인을 통해 자원 검증 및 소모를 최상단 관문에서 처리합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 순정 로깅 인프라: 개발 모드에서 궁극기 격발 상태를 시각적으로 추적합니다.
    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("Kaguya Ultimate Activated: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    // 자신 중심 12미터 광역 스캔 및 천인 강림 가동 구역
    FVector KaguyaLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    // 기획 데이터 락인: 에디터 변수인 AscensionRadius 수치를 실시간 참조하여 구체 형태의 스캔 범위를 결정합니다.
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(AscensionRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Kaguya Ultimate: 달의 군대여 이 땅의 혼란을 가라앉히소서."));
    }

    // 가시성 및 폰 채널 기반의 광역 폰 스캔 격발
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
                // 이중 방어선: 블루/레드 팀 태그 대조를 통해 광역기 판정에서 아군 오사 피해를 완벽하게 배제합니다.
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;
                
                // 에셋 바인딩 가이드: 대미지 이펙트 에셋
                // 에디터 내 DamageGameplayEffectClass 슬롯에 광역 피해용 이펙트 에셋을 장착하십시오.
                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }

                // AOS 예외 처리 규격: 포탑이나 넥서스 구조체에는 강제 이동이 불가능하므로 구조체 태그 보유 여부를 확인합니다.
                bool bIsStructure = TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret) || 
                                    TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus);

                // 구조체가 아닌 영웅 및 미니언 대상의 물리 견인 및 디버프 파이프라인 개통
                if (!bIsStructure)
                {
                    // 중심부 강제 인장 (Pull Mechanism)
                    ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
                    if (TargetCharacter)
                    {
                        FVector TargetLocation = TargetCharacter->GetActorLocation();
                        
                        FVector PullDirection = (KaguyaLocation - TargetLocation);
                        PullDirection.Z = 0.f; // Z축 물리 왜곡을 원천 방어합니다.
                        
                        float Distance = PullDirection.Size();
                        
                        // 물리 버그 방어선: 너무 가까워 이미 캐릭터가 완전히 겹친 상태라면 인장력을 0으로 처리하여 맵 밖 사출 크래시를 차단합니다.
                        if (Distance > 15.0f)
                        {
                            PullDirection.Normalize();

                            // 거리에 따른 인장력 가중치 연산 후 최대 임계치를 설정하여 안정성을 유지합니다.
                            float CalculatedForce = Distance * PullForceMultiplier;
                            CalculatedForce = FMath::Clamp(CalculatedForce, 300.0f, 2500.0f);

                            FVector LaunchVelocity = PullDirection * CalculatedForce;
                            LaunchVelocity.Z = 100.f; // 가볍게 띄워 중앙으로 견인합니다.

                            // 캐릭터 무브먼트 내장 기능을 활용하여 튕김 없는 안전한 물리 격발을 시행합니다.
                            TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                        }
                    }

                    // 2초간 80퍼센트 슬로우 디버프 주입
                    // 에셋 바인딩 가이드: 에디터 내 SlowDebuffEffectClass 슬롯에 둔화 이펙트 에셋을 장착하십시오.
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

    // 궁극기 로직이 정상 완수되면 즉시 능력을 종료합니다.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}