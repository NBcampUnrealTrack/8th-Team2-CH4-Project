// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/FT_AttributeSet.h"

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

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

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
    
    FireClockRabbit();
    
    if (!bHasVisualTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();

    // 💡 RabbitImpactEffectClass 자리에 DamageEffectClass로 검문소 스왑
    if (World && ClockRabbitProjectileClass && DamageEffectClass && Character)
    {
        FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.0f + FVector(0, 0, 40.0f);
        FVector LaunchDirection = Character->GetActorForwardVector();

        if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
        {
            UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
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
        }

        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        // 💡 공용 대미지 GE를 기반으로 아웃고잉 스펙 핸들 생성
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
        {
            // 시전자 정보를 컨텍스트에 완벽 수립
            FGameplayEffectContextHandle SkillContext = SpecHandle.Data->GetContext();
            SkillContext.AddSourceObject(Character);
            SkillContext.AddInstigator(Character, Character);
            
            SpecHandle.Data->SetContext(SkillContext.Duplicate());

            // 💡 앨리스 디폴트 베이스 대미지(100.0)를 SetByCaller로 직격 주입
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
        
        FGameplayTagContainer TargetCooldownTags;
        TargetCooldownTags.AddTag(CooldownTag);
        
        FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
        MyASC->RemoveActiveEffects(CooldownQuery);
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}