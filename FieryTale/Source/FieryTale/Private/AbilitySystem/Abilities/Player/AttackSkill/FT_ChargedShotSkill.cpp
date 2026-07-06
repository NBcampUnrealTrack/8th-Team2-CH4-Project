// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
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

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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
    bool bTriggeredVisualTask = false;

    if (Character)
    {
        float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;

        // 1초 이상 마우스 홀드 시 풀 차징 성공
        if (ChargeDuration >= 1.0f)
        {
            CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
            
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                UWorld* World = GetWorld();
                
                if (World && WeaponData->ProjectileClass && DamageEffectClass)
                {
                    FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                    FVector LaunchDirection = Character->GetActorForwardVector();
                    FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = Character;
                    SpawnParams.Instigator = Character;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    // 풀 차징 종합 폭발 계산서(Spec) 조립
                    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
                    if (SpecHandle.IsValid())
                    {
                        // ① 풀 차징 폭발 피해량(50.0f) 기입
                        SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                        
                        // ② [넉백 데이터 동시 전송 배관] 
                        // 만약 필요하시다면 전투용 공용 넉백 태그(예: FTTags::FTCombat::Knockback) 등을 활용해 
                        // 계산서에 넉백 물리 강도(800.0f)까지 밀봉 주입하여 연동할 수 있습니다.
                    }

                    // 지연 생성 체계를 경유하여 지니의 폭발 주먹 투사체 안전 사출
                    AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                        WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                    
                    if (Projectile)
                    {
                        // 완성된 폭발/넉백 계산서를 투사체 내부 배관에 토스
                        Projectile->DamageEffectSpecHandle = SpecHandle;
                        
                        // 사출 완료
                        Projectile->FinishSpawning(SpawnTransform);
                    }
                    
                    if (FireMontage)
                    {
                        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                            this, TEXT("AladdinFireTask"), FireMontage, 1.0f);

                        if (MontageTask)
                        {
                            // 완료, 방해, 강제 취소 델리게이트를 전부 묶어 관리합니다.
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
        else
        {
            // 1초 미만 불완전 차징 취소 구역
        }
    }
    
    // 몽타주 연출이 온전히 끝나서 바인딩된 온파이어 콜백이 가동되기 전까지 어빌리티 생명을 가두어둡니다.
    if (!bTriggeredVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::OnFireMontageFinished()
{
    // 지니의 주먹 강타 액션 잔상이 완전히 마감된 순간 장부를 클린하게 정리합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_ChargedShotSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}