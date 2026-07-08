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
    // 이를 통해 우클릭 Cooldown GE 장부가 굳건하게 잠기기 시작합니다.
    // 만약 중간에 CC기를 맞아 취소된다면 EndAbility에서 무결하게 환불 처리합니다.
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
    // 타이머가 만료되어 도달했으므로 핸들 장부만 깔끔하게 청소 초기화합니다.
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
    // CC기를 맞아 끊겼으므로 예약 타이머를 청정하게 즉시 파쇄합니다.
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
    // 어떤 경로로든 어빌리티가 닫힐 때 예외 없이 타이머를 소각하여 레이스 컨디션을 방지합니다.
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

        // =========================================================================
        // [쿨타임 무결성 환불]: 1초 채널링 완수를 못 하고 군중제어기에 끊겼다면(bWasCancelled),
        // 선제 적용되었던 9초 우클릭 쿨타임 이펙트를 찾아내 추적 삭제 장부에서 제거합니다.
        // =========================================================================
        if (bWasCancelled)
        {
            FGameplayEffectQuery UniversalQuery;
            TArray<FActiveGameplayEffectHandle> ActiveHandles = SourceASC->GetActiveEffects(UniversalQuery);

            for (const FActiveGameplayEffectHandle& GEPipeHandle : ActiveHandles)
            {
                const FActiveGameplayEffect* ActiveGE = SourceASC->GetActiveGameplayEffect(GEPipeHandle);
                if (ActiveGE && ActiveGE->Spec.Def)
                {
                    if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                        ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                    {
                        SourceASC->RemoveActiveGameplayEffect(GEPipeHandle);
                    }
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}