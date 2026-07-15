// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    // 개체별 상태 추적을 위해 인스턴싱 정책을 적용합니다.
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
        UE_LOG(LogTemp, Log, TEXT("WolfRoar Activated: 할머니를 삼켰던 이빨로 적들을 물어뜯어라."));
    }

    // 서버 권한에서만 대미지 및 디버프를 적용하여 중복 처리를 방지합니다.
    if (HasAuthority(&ActivationInfo))
    {
        FVector StartLocation = OwnerActor->GetActorLocation();
        FVector ForwardVector = OwnerActor->GetActorForwardVector();
        
        ForwardVector.Z = 0.f; // Z축 변위를 0으로 고정하여 수평 방향을 유지합니다.
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

                // 루프 외부에서 이펙트 인스턴스를 한 번만 생성하여 최적화합니다.
                FGameplayEffectSpecHandle MasterDamageSpecHandle;
                FGameplayEffectSpecHandle MasterRootSpecHandle;

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

                // 2. 속박 디버프 이펙트 인스턴스 생성
                if (RootGameplayEffectClass)
                {
                    MasterRootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                    if (MasterRootSpecHandle.IsValid() && MasterRootSpecHandle.Data.IsValid())
                    {
                        MasterRootSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                        MasterRootSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);
                    }
                }

                // 부채꼴 범위 내의 대상을 검출하고 이펙트를 적용합니다.
                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* TargetActor = Result.GetActor();
                    if (!IsValid(TargetActor))
                    {
                        continue;
                    }

                    // 도트 내적 계산을 통해 자신 앞의 일정 각도 내에 있는지 확인합니다.
                    FVector DirectionToTarget = (TargetActor->GetActorLocation() - OwnerActor->GetActorLocation()).GetSafeNormal();
                    float CurrentDot = FVector::DotProduct(ForwardVector, DirectionToTarget);
                    
                    if (CurrentDot < DotThreshold)
                    {
                        continue;
                    }

                    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
                    if (TargetASC && SourceASC)
                    {
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
                        {
                            continue;
                        }
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
                        {
                            continue;
                        }
                        
                        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
                        {
                            continue;
                        }

                        // 공통 히트 결과를 생성합니다.
                        FHitResult IndividualHit;
                        IndividualHit.HitObjectHandle = TargetActor;
                        IndividualHit.Location = TargetActor->GetActorLocation();
                        IndividualHit.ImpactPoint = TargetActor->GetActorLocation();

                        // 1) 생성된 이펙트 인스턴스를 재사용하여 대미지를 적용합니다.
                        if (MasterDamageSpecHandle.IsValid() && MasterDamageSpecHandle.Data.IsValid())
                        {
                            FGameplayEffectSpec LocalDamageSpec(*MasterDamageSpecHandle.Data.Get());
                            FGameplayEffectContextHandle LocalContext = LocalDamageSpec.GetContext().Duplicate();
                            LocalContext.AddHitResult(IndividualHit, true);
                            LocalDamageSpec.SetContext(LocalContext);

                            SourceASC->ApplyGameplayEffectSpecToTarget(LocalDamageSpec, TargetASC);
                        }

                        // 2) 생성된 이펙트 인스턴스를 재사용하여 속박 디버프를 적용합니다.
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
    
    if (SkillMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("WolfRoarMontage"), SkillMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_WolfRoarAbility::OnMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_WolfRoarAbility::OnMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_WolfRoarAbility::OnMontageFinished);
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

void UFT_WolfRoarAbility::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_WolfRoarAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 어빌리티 종료 시 궁극기 태그를 제거합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}