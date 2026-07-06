// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : ChargeStartTime(0.0f)
    , BaseDamage(50.0f)      
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

    // 마나 비용이 없으므로, 현재 9초 쿨타임 태그가 걸려있는지만 순정 GAS 검문소에서 체크합니다.
    // 만약 아직 쿨다운 태그가 남아있다면 이 자리에서 즉시 활성화를 차단하고 탈출합니다.
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        ChargeStartTime = World->GetTimeSeconds();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::FireChargedShot()
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
        float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;

        // 1단계: 1초 이상 마우스 홀드 시 풀 차징 사출 성공 분기
        if (ChargeDuration >= 1.0f)
        {
            // 1초 차징 조준을 무사히 성공 완수한 바로 이 시점에 쿨타임을 정식으로 격발 낙인찍습니다.
            CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
            
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
        // 2단계: 1초 미만 불완전 차징 취소 분기 처리 (패널티 없는 청정 복귀)
        else
        {
            // 애초에 소모한 마나 자원 자체가 없으므로 환불 연산 연동 코드도 전량 필요 없습니다.
            // 쿨타임 패널티를 가동하지 않고 안전하게 취소 플래그만 들고 스킬을 원점 종료합니다.
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