// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AliceUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Engine/Engine.h"

UFT_AliceUltimateAbility::UFT_AliceUltimateAbility()
{
    // 본체 개체별 독립적인 상태 추적 및 타이머 제어를 위해 인스턴싱 정책을 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    // 시전 중임을 알리는 태그를 채널링 동안 소유하도록 락인합니다.
    ActivationOwnedTags.AddTag(FTTags::FTCombat::Skill_Channelling);

    // 반경 기본값 설정
    TimeStopRadius = 600.0f;
}

void UFT_AliceUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 부모 클래스의 ActivateAbility를 호출합니다.
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 조건을 검증합니다.
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

    if (SkillMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("AliceUltMontage"), SkillMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->ReadyForActivation();
        }
    }

    // 광역 시간 정지 대상을 검색합니다. 서버 권한에서만 실행하여 중복 처리를 방지합니다.
    if (OwnerActor->HasAuthority())
    {
        FVector CenterLocation = OwnerActor->GetActorLocation();
        TArray<FOverlapResult> OverlapResults;
        FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
        if (GetWorld())
        {
            DrawDebugSphere(GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
        }
#endif

        if (GetWorld())
        {
            bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
                OverlapResults, CenterLocation, FQuat::Identity, ECollisionChannel::ECC_Pawn, ScanSphere, QueryParams);

            if (bHasOverlap && TimeStopDebuffEffectClass)
            {
                // 루프 외부에서 디버프 이펙트 인스턴스를 한 번만 생성하여 최적화합니다.
                FGameplayEffectSpecHandle MasterDebuffSpecHandle = MakeOutgoingGameplayEffectSpec(TimeStopDebuffEffectClass, GetAbilityLevel());
                
                if (MasterDebuffSpecHandle.IsValid() && MasterDebuffSpecHandle.Data.IsValid())
                {
                    MasterDebuffSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                    MasterDebuffSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);

                    for (const FOverlapResult& Result : OverlapResults)
                    {
                        AActor* TargetActor = Result.GetActor();
                        if (!IsValid(TargetActor))
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

                            // 생성된 이펙트 인스턴스를 재사용하여 디버프를 적용합니다.
                            FGameplayEffectSpec LocalSpec(*MasterDebuffSpecHandle.Data.Get());
                            FGameplayEffectContextHandle LocalContext = LocalSpec.GetContext().Duplicate();

                            FHitResult IndividualHit;
                            IndividualHit.HitObjectHandle = TargetActor;
                            IndividualHit.Location = TargetActor->GetActorLocation();
                            IndividualHit.ImpactPoint = TargetActor->GetActorLocation();
                            
                            LocalContext.AddHitResult(IndividualHit, true);
                            LocalSpec.SetContext(LocalContext);
                            
                            SourceASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
                        }
                    }
                }
            }
        }
    }

    // 타이머를 설정하여 어빌리티 수명을 동기화합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AliceReleaseTimerHandle, 
            this, 
            &UFT_AliceUltimateAbility::ReleaseAlice, 
            1.0f, 
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AliceUltimateAbility::ReleaseAlice()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AliceReleaseTimerHandle);
        AliceReleaseTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 어빌리티 종료 시 궁극기 태그를 제거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);

        // 어빌리티가 취소되었을 경우 쿨타임을 초기화합니다.
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            // 해당 쿨타임 태그를 가진 활성 이펙트를 제거합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}