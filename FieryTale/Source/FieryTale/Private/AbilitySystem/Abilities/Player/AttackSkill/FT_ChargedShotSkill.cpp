// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h" // 릴리즈 감시 태스크 인클루드 완착

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : BaseDamage(50.0f)      
    , KnockbackForce(800.0f) 
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_ChargedShotSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1단계: 9초 쿨타임 검문
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!GetWorld())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // =========================================================================
    // [인풋 릴리즈 감시선 개통]: 마우스 버튼을 떼는 이벤트를 비동기 추적합니다.
    // 플레이어가 버튼을 누르고 있던 총 시간이 OnRelease 델리게이트를 통해 사출됩니다.
    // =========================================================================
    UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    if (ReleaseTask)
    {
        ReleaseTask->OnRelease.AddDynamic(this, &UFT_ChargedShotSkill::FireChargedShot);
        ReleaseTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

// 💡 [컴파일 릭 완치]: 헤더 장부와 일치하도록 float TimePressed 매개변수 파이프라인을 연결했습니다.
void UFT_ChargedShotSkill::FireChargedShot(float TimePressed)
{
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid() || !GetWorld())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
    bool bTriggeredVisualTask = false;

    if (Character && SourceASC)
    {
        // [기획 사양 완착]: 수동 시간 계산 장부를 들어내고, 태스크가 실측한 TimePressed를 직격 제어합니다.
        if (TimePressed >= 1.0f)
        {
            // 1초 이상 풀 차징 달성 순간에 정석대로 활성화 장부 마킹 및 9초 쿨타임 GE 강제 착륙
            if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
            {
                EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
                return;
            }
            
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                UWorld* World = GetWorld();
                
                if (World && WeaponData->ProjectileClass && DamageEffectClass)
                {
                    FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                    FVector LaunchDirection = Character->GetActorForwardVector();
                    FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
                    if (SpecHandle.IsValid())
                    {
                        SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                        SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Knockback, KnockbackForce);
                    }

                    AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                        WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                    
                    if (Projectile)
                    {
                        Projectile->DamageEffectSpecHandle = SpecHandle;
                        Projectile->FinishSpawning(SpawnTransform);
                    }
                    
                    if (FireMontage)
                    {
                        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                            this, TEXT("AladdinFireTask"), FireMontage, 1.0f);

                        if (MontageTask)
                        {
                            MontageTask->OnCompleted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            MontageTask->OnInterrupted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            MontageTask->OnCancelled.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            
                            MontageTask->ReadyForActivation();
                            bTriggeredVisualTask = true;
                        }
                    }
                }
            }
        }
        // 1초 미만 불완전 차징 낙폭 분기 (패널티 제로 원점 회군)
        else
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
            return;
        }
    }
    
    if (!bTriggeredVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::OnFireMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_ChargedShotSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}