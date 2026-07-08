// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

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

    // [GAS 예측 아키텍처 규격 확정]: 어빌리티 가동 최초 시점에 자원 원자적 선커밋 타설 완료
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!GetWorld())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // [인풋 릴리즈 네트워크 복제 활성화] 조준 해제 패킷 동기화 선로 개통
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

void UFT_ChargedShotSkill::FireChargedShot(float TimePressed)
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
        // 💡 [1초 정밀 충전 성공 판정선]
        if (TimePressed >= 1.0f)
        {
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
        // 💡 [차징 실패 낙폭 구역 - 패널티 프리 면제 환원 수선]
        else
        {
            if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
            {
                UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
                if (MyASC)
                {
                    // =========================================================================
                    // 💡 [언리얼 순정 GAS 쿨타임 철거 공정 완착]:
                    // 컴파일 에러를 유발하던 유령 API선들을 완전히 파쇄 소각하고,
                    // 앨리스 스킬 장부에서 100% 무결성이 증명된 정석 추출 순회망을 락인합니다.
                    // 에셋 태그든 부여 태그든 CooldownTag를 공차 없이 완전히 제거합니다.
                    // =========================================================================
                    FGameplayEffectQuery CooldownQuery;
                    TArray<FActiveGameplayEffectHandle> ActiveHandles = MyASC->GetActiveEffects(CooldownQuery);
                    TArray<FActiveGameplayEffectHandle> HandlesToRemove;

                    for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
                    {
                        const FActiveGameplayEffect* ActiveGE = MyASC->GetActiveGameplayEffect(Handle);
                        if (ActiveGE && ActiveGE->Spec.Def)
                        {
                            if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                                ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                            {
                                HandlesToRemove.Add(Handle);
                            }
                        }
                    }

                    // 수집 안전 지대에서 원자적 소각 마감
                    for (const FActiveGameplayEffectHandle& TargetHandle : HandlesToRemove)
                    {
                        MyASC->RemoveActiveGameplayEffect(TargetHandle);
                    }
                }
            }
            
            // [재귀적 인터럽트 버그 박멸 완치선]: 안전한 기획적 정상 마감 처리
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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