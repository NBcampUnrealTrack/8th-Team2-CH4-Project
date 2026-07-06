// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h" 
#include "DrawDebugHelpers.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Engine/OverlapResult.h"

UFT_AliceSkill::UFT_AliceSkill()
    : BaseDamage(30.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 마나 자원이 없으므로 현재 9초 쿨타임 태그가 걸려있는지만 순정 GAS 관문에서 체크합니다.
    // 쿨타임 중이라면 아래의 레이더 스캔과 투사체 사출을 원천 차단하고 즉시 탈출합니다.
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    // 1단계: 동적 해킹 레이더 기동 및 적군 필터 스캔 마킹
    ScanHackingTargets();

    // 2단계: 비동기 몽타주 재생 및 분기형 콜백 배관망 가동
    bool bHasVisualTask = false;
    if (AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("AliceSkillTask"), AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 배관 분리: 정상 완료 시와 방해/취소 당했을 때의 수신 파이프라인을 완전히 쪼개어 연동합니다.
            MontageTask->OnCompleted.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            
            MontageTask->ReadyForActivation();
            bHasVisualTask = true;
        }
    }
    
    // 3단계: 실물 투사체 사출 (정상적인 흐름에서 격발)
    FireClockRabbit();
    
    if (!bHasVisualTask)
    {
        // 비주얼 태스크가 없다면 사출 직후 즉시 쿨타임을 확정 차감하고 종료합니다.
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_AliceSkill::ScanHackingTargets()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();
    if (!Character || !World) return;
    
    UAbilitySystemComponent* OwnerASC = CurrentActorInfo->AbilitySystemComponent.Get();
    if (!OwnerASC) return;

    FGameplayTag TargetEnemyTag = FGameplayTag::EmptyTag;

    if (OwnerASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
    {
        TargetEnemyTag = FTTags::FTFaction::Team_Red;
    }
    else if (OwnerASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
    {
        TargetEnemyTag = FTTags::FTFaction::Team_Blue;
    }
    
    if (!TargetEnemyTag.IsValid()) return;
    
    TrackedTargets.Empty();
    
    const float ScanRadius = 800.0f;
    FVector ScanCenter = Character->GetActorLocation();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);
    
    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = World->OverlapMultiByChannel(
        OverlapResults,
        ScanCenter,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(ScanRadius),
        QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* OverlappedActor = Result.GetActor();
            if (!OverlappedActor) continue;

            if (IAbilitySystemInterface* GASInterface = Cast<IAbilitySystemInterface>(OverlappedActor))
            {
                if (UAbilitySystemComponent* TargetASC = GASInterface->GetAbilitySystemComponent())
                {
                    if (TargetASC->HasMatchingGameplayTag(TargetEnemyTag))
                    {
                        TrackedTargets.Add(OverlappedActor);
                        TargetASC->AddLooseGameplayTag(FTTags::FTStates::Buff::HackedMark);
                    }
                }
            }
        }
    }
    
#if !UE_BUILD_SHIPPING
    DrawDebugSphere(World, ScanCenter, ScanRadius, 16, FColor::Cyan, false, 1.0f, 0, 1.5f);
#endif
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character || !Character->GetWeaponData()) return;

    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    UWorld* World = GetWorld();

    if (World && WeaponData->ProjectileClass && RabbitImpactEffectClass)
    {
        FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
        FVector LaunchDirection = Character->GetActorForwardVector();
        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RabbitImpactEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = SpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
    }
}

void UFT_AliceSkill::OnSkillMontageFinished()
{
    // 스킬 모션 연출이 도중에 방해받지 않고 온전히 성공 완료된 "이 최종 시점"에만 9초 쿨타임을 명시적으로 격발시킵니다.
    CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::HandleSkillInterrupted()
{
    // 시전 도중 하드 CC기를 처맞아 끊긴 경우, 사출 로직을 파쇄하고 쿨타임 패널티 없이 취소 플래그(bWasCancelled = true)를 들고 즉시 청정 종료합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_AliceSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 스킬 수명 주기가 끝나는 순간 포착했던 적들의 머리 위 해킹 표식을 깨끗하게 수거 복구합니다.
    for (TObjectPtr<AActor> TargetActor : TrackedTargets)
    {
        if (::IsValid(TargetActor))
        {
            if (IAbilitySystemInterface* GASInterface = Cast<IAbilitySystemInterface>(TargetActor))
            {
                if (UAbilitySystemComponent* TargetASC = GASInterface->GetAbilitySystemComponent())
                {
                    TargetASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::HackedMark);
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}