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
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_KaguyaUltimateAbility::UFT_KaguyaUltimateAbility()
{
    // 개체별 상태 추적을 위해 인스턴싱 정책을 적용합니다.
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
    // 부모 클래스의 ActivateAbility를 호출합니다.
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 조건 및 코스트를 검증합니다.
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

    // 광역 스캔 영역을 설정합니다.
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
        
        // 서버 권한 환경에서만 대미지, 디버프, 물리 연산을 적용하여 중복 처리를 방지합니다.
        if (bHasOverlap && OwnerActor->HasAuthority())
        {
            // 루프 외부에서 이펙트 인스턴스를 한 번만 생성하여 최적화합니다.
            FGameplayEffectSpecHandle MasterDamageSpecHandle;
            FGameplayEffectSpecHandle MasterSlowSpecHandle;

            // 1. 대미지 이펙트 인스턴스 생성
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

            // 2. 이속 감소 이펙트 인스턴스 생성
            if (SlowDebuffEffectClass)
            {
                MasterSlowSpecHandle = MakeOutgoingGameplayEffectSpec(SlowDebuffEffectClass, GetAbilityLevel());
                if (MasterSlowSpecHandle.IsValid() && MasterSlowSpecHandle.Data.IsValid())
                {
                    MasterSlowSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                    MasterSlowSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                }
            }

            // 오버랩된 대상에 대해 이펙트를 적용합니다.
            for (const FOverlapResult& Result : OverlapResults)
            {
                AActor* HitActor = Result.GetActor();
                if (!IsValid(HitActor))
                {
                    continue;
                }

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
                if (TargetASC && SourceASC)
                {
                    // 피아 식별 필터 레이어를 가동하여 아군 타격 피해를 완벽히 격리 차단합니다.
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
                    {
                        continue;
                    }
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
                    {
                        continue;
                    }
                    
                    // 이미 사망 상태인 타깃 시체 구타 차단
                    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
                    {
                        continue;
                    }

                    // 공통 피격 히트 결과 장부 스냅샷 생성
                    FHitResult IndividualHit;
                    IndividualHit.HitObjectHandle = HitActor;
                    IndividualHit.Location = HitActor->GetActorLocation();
                    IndividualHit.ImpactPoint = HitActor->GetActorLocation();

                    // 1) 생성된 이펙트 인스턴스를 재사용하여 대미지를 적용합니다.
                    if (MasterDamageSpecHandle.IsValid() && MasterDamageSpecHandle.Data.IsValid())
                    {
                        FGameplayEffectSpec LocalDamageSpec(*MasterDamageSpecHandle.Data.Get());
                        FGameplayEffectContextHandle LocalContext = LocalDamageSpec.GetContext().Duplicate();
                        LocalContext.AddHitResult(IndividualHit, true);
                        LocalDamageSpec.SetContext(LocalContext);

                        SourceASC->ApplyGameplayEffectSpecToTarget(LocalDamageSpec, TargetASC);
                    }

                    // 구조물(포탑, 넥서스)인지 확인합니다.
                    bool bIsStructure = TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret) || 
                                        TargetASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus);

                    // 구조물이 아닌 캐릭터일 경우에만 당기기 및 디버프를 적용합니다.
                    if (!bIsStructure)
                    {
                        // 2) 중심부 강제 인장 물리 제어 (Pull Mechanism)
                        ACharacter* TargetCharacter = Cast<ACharacter>(HitActor);
                        if (IsValid(TargetCharacter))
                        {
                            FVector TargetLocation = TargetCharacter->GetActorLocation();
                            FVector PullDirection = (KaguyaLocation - TargetLocation);
                            PullDirection.Z = 0.f; // Z축 변위를 0으로 고정하여 물리 연산 오류를 방지합니다.
                            
                            float Distance = PullDirection.Size();
                            
                            if (Distance > 15.0f)
                            {
                                PullDirection.Normalize();

                                float CalculatedForce = Distance * PullForceMultiplier;
                                CalculatedForce = FMath::Clamp(CalculatedForce, 300.0f, 2500.0f);

                                FVector LaunchVelocity = PullDirection * CalculatedForce;
                                LaunchVelocity.Z = 100.f; // 에어본 효과 적용

                                TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
                            }
                        }

                        // 3) 생성된 이펙트 인스턴스를 재사용하여 디버프를 적용합니다.
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

    UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("KaguyaUltMontage"), LoadedMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_KaguyaUltimateAbility::OnMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_KaguyaUltimateAbility::OnMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_KaguyaUltimateAbility::OnMontageFinished);
            MontageTask->ReadyForActivation();
        }
        else
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_KaguyaUltimateAbility::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 어빌리티 종료 시 궁극기 태그를 제거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}