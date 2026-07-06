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
                // 채널링 도중 적의 군중제어기 공격을 받아 애니메이션이 끊기면 투사체 사출 없이 능력을 강제 취소 종료합니다.
                MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->ReadyForActivation();
            }
        }
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 1초 동안 아무런 방해 없이 조준에 성공하면 최종 격발 함수(FireSilverBullet)로 토스합니다.
        World->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    // 1초 채널링 완공 시점에 조준 타이머를 청정하게 제거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
    }

    // 투사체 실제 월드 사출 공정 집도
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

    // 은탄 사출이 끝난 바로 이 최종 시점에 능력을 정상 마감하여 쿨타임 태그가 올바르게 추적 유지되도록 합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::HandleChannellingInterrupted()
{
    // 채널링 도중 CC기를 맞아 파쇄된 경우, 사출 로직을 건너뛰고 취소 플래그(bWasCancelled = true)를 들고 즉시 종료 관문으로 이동합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}