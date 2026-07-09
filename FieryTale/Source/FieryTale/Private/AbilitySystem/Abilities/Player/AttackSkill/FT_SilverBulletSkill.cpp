// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_SilverBulletSkill::UFT_SilverBulletSkill()
    : BaseDamage(50.0f)
    , ChannellingDuration(1.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_SilverBulletSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1단계: 우클릭 쿨타임 검문
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // =========================================================================
    // [쿨타임 완치 배관]: 채널링 진입 시점에 CommitAbility 마스터 함수를 가동합니다.
    // =========================================================================
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (SourceASC)
    {
        SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }
    
    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        if (WeaponData && WeaponData->AttackMontage)
        {
            UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this, TEXT("SilverBulletChannellingTask"), WeaponData->AttackMontage, 1.0f);

            if (MontageTask)
            {
                // 채널링 도중 CC기를 맞아 끊기면 HandleChannellingInterrupted 콜백으로 강제 취소 종료
                MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->ReadyForActivation();
            }
        }
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 1초 동안 조준 성공 시 최종 격발 함수로 토스
        World->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    ChannellingTimerHandle.Invalidate();

    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
        if (Character && Character->GetWeaponData())
        {
            UFT_WeaponData* WeaponData = Character->GetWeaponData();
            UWorld* World = GetWorld();
            
            if (World && WeaponData->ProjectileClass && SilverBulletImpactEffectClass)
            {
                FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                FVector LaunchDirection = Character->GetActorForwardVector();
                FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SilverBulletImpactEffectClass, GetAbilityLevel());
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
    }

    // 사출 완료 시 정상 종료 (bWasCancelled = false 이므로 쿨타임 유지)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::HandleChannellingInterrupted()
{
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

    // bWasCancelled = true 사인을 들고 조기 종료 파이프라인으로 진입 (이속 및 쿨타임 환불 처리 유도)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

        if (bWasCancelled)
        {
            FGameplayEffectQuery UniversalQuery;
            TArray<FActiveGameplayEffectHandle> ActiveHandles = SourceASC->GetActiveEffects(UniversalQuery);

            // 💡 [메모리 컨테이너 무결성 타설망]
            // 실시간 소각 루프를 파쇄하고, 제거 대상 핸들들을 안전 구역 격리 어레이에 수집합니다.
            TArray<FActiveGameplayEffectHandle> HandlesToRemove;

            for (const FActiveGameplayEffectHandle& GEPipeHandle : ActiveHandles)
            {
                const FActiveGameplayEffect* ActiveGE = SourceASC->GetActiveGameplayEffect(GEPipeHandle);
                if (ActiveGE && ActiveGE->Spec.Def)
                {
                    if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                        ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                    {
                        HandlesToRemove.Add(GEPipeHandle);
                    }
                }
            }

            // 안전 루프 탈출 후 원자적으로 소각을 완수하여 메모리 폭사를 완벽히 진압합니다.
            for (const FActiveGameplayEffectHandle& TargetHandle : HandlesToRemove)
            {
                SourceASC->RemoveActiveGameplayEffect(TargetHandle);
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}