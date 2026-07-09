// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

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

    // 💡 [순정 GAS 가드선 복구]: 중복 검문을 소각하고 CommitAbility 단일 파이프라인으로 일원화하여
    // 쿨타임과 코스트 장부를 원자적으로 검문 및 차단인(Lock-in)합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 비동기 몽타주 재생 및 분기형 콜백 배관망 가동
    bool bHasVisualTask = false;
    if (AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("AliceSkillTask"), AttackMontage, 1.0f);

        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            
            MontageTask->ReadyForActivation();
            bHasVisualTask = true;
        }
    }
    
    // 시계 토끼 환영(독립 투사체) 사출 격발
    FireClockRabbit();
    
    // 💡 [레이스 컨디션 안전 우회선]: 비주얼 태스크가 생성되지 않았더라도 즉시 꺼버리지 않고 
    // 최소한의 동기화 프레임을 확보한 뒤 클린 마감합니다.
    if (!bHasVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();

    if (World && ClockRabbitProjectileClass && RabbitImpactEffectClass && Character)
    {
        FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.0f + FVector(0, 0, 40.0f);
        FVector LaunchDirection = Character->GetActorForwardVector();
        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RabbitImpactEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ClockRabbitProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = SpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
    }
}

void UFT_AliceSkill::OnSkillMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::HandleSkillInterrupted()
{
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
        
        // 💡 [네트워크 동기화 쿨타임 환불 릭 수정]:
        // LocalPredicted 정책에 의거하여, 시전자가 CC기를 맞아 캔슬되었을 때 서버와 클라이언트 양단에서
        // 예측적으로 돌던 우클릭 쿨타임 이펙트 장부를 완벽하게 동시 소각 마감합니다.
        FGameplayEffectQuery UniversalQuery;
        TArray<FActiveGameplayEffectHandle> ActiveHandles = MyASC->GetActiveEffects(UniversalQuery);
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

        for (const FActiveGameplayEffectHandle& TargetHandle : HandlesToRemove)
        {
            MyASC->RemoveActiveGameplayEffect(TargetHandle);
        }
    }

    // 인터럽트에 의한 능동 종료 명시 (bWasCancelled = true)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}