// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h" 
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

    // [1단계: 관문 검문] 쿨타임 상태라면 즉시 어빌리티 가동을 차단합니다.
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
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
    
    if (!bHasVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();

    // [2번째 사양 반영 완료]: WeaponData 참조 장부를 완전히 끊고, 헤더의 ClockRabbitProjectileClass를 직격 타겟팅합니다.
    if (World && ClockRabbitProjectileClass && RabbitImpactEffectClass)
    {
        FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.0f + FVector(0, 0, 40.0f);
        FVector LaunchDirection = Character->GetActorForwardVector();
        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RabbitImpactEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        // 평타 에셋이 아닌 오직 이 스킬 창에서 세팅한 전용 시계 토끼 실체 클래스를 Deferred 소환합니다.
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
        
        FGameplayEffectQuery UniversalQuery;
        TArray<FActiveGameplayEffectHandle> ActiveHandles = MyASC->GetActiveEffects(UniversalQuery);

        for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
        {
            const FActiveGameplayEffect* ActiveGE = MyASC->GetActiveGameplayEffect(Handle);
            if (ActiveGE && ActiveGE->Spec.Def)
            {
                if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                    ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                {
                    MyASC->RemoveActiveGameplayEffect(Handle);
                }
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}