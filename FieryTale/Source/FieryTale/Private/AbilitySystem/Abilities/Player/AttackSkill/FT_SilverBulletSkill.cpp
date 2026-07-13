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
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/FT_AttributeSet.h"

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
                MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->ReadyForActivation();
            }
        }
    }

    UWorld* World = GetWorld();
    if (World)
    {
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

    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid() && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
        UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();

        if (Character && MyASC && Character->GetWeaponData())
        {
            UFT_WeaponData* WeaponData = Character->GetWeaponData();
            UWorld* World = GetWorld();
            
            // 💡 [수선 완료]: SilverBulletImpactEffectClass 자리에 DamageEffectClass 배관 타설
            if (World && WeaponData->ProjectileClass && DamageEffectClass)
            {
                FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                FVector LaunchDirection = Character->GetActorForwardVector();

                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                if (AttributeSet)
                {
                    const float CurrentSpread = AttributeSet->GetWeaponSpread();
                    if (CurrentSpread > 0.0f)
                    {
                        const float ConeHalfAngleRadius = FMath::DegreesToRadians(CurrentSpread);
                        LaunchDirection = FMath::VRandCone(LaunchDirection, ConeHalfAngleRadius);
                    }
                }

                FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                // 💡 공용 대미지 GE를 기반으로 아웃고잉 스펙 핸들 생성
                FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
                if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
                {
                    // 💡 [GEEC 연산소 완착 트리거]: 시전자 정보(Source/Instigator)를 컨텍스트 장부에 물리적으로 완전히 주입합니다.
                    FGameplayEffectContextHandle SkillContext = SpecHandle.Data->GetContext();
                    SkillContext.AddSourceObject(Character);
                    SkillContext.AddInstigator(Character, Character);
                    
                    // 깊은 복사를 거친 컨텍스트 락인
                    SpecHandle.Data->SetContext(SkillContext.Duplicate());

                    // 실버불릿 스킬 고유 베이스 대미지 주입 (50.0f)
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

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::HandleChannellingInterrupted()
{
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

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
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}