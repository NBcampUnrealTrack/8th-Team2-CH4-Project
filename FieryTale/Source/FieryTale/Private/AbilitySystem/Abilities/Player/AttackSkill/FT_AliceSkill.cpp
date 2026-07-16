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
    UAnimMontage* LoadedMontage = nullptr;
    if (!AttackMontage.IsNull())
    {
        LoadedMontage = AttackMontage.LoadSynchronous();
    }
    
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("AliceSkillTask"), LoadedMontage, 1.0f);

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

    // 투사체 에셋을 비동기(또는 동기) 로드하여 메모리에 적재합니다.
    UClass* LoadedProjectileClass = ClockRabbitProjectileClass.LoadSynchronous();

    // 투사체를 스폰합니다.
    if (World && LoadedProjectileClass && DamageEffectClass && Character)
    {
        // 스폰 위치 및 방향 계산
        FVector SpawnLocation = Character->GetWeaponMuzzleLocation();
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
        SpawnTransform.SetRotation(FQuat(LaunchDirection.Rotation()));

        // 대미지 이펙트 스펙 생성
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
        {
            // 시전자 정보 설정
            FGameplayEffectContextHandle SkillContext = SpecHandle.Data->GetContext();
            SkillContext.AddSourceObject(Character);
            SkillContext.AddInstigator(Character, Character);
            
            SpecHandle.Data->SetContext(SkillContext.Duplicate());

            // 피해량 설정
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }
        
        if (Character->HasAuthority())
        {
            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                LoadedProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                Projectile->DamageEffectSpecHandle = SpecHandle;

                // 추가 디버프(슬로우, 인장 등) 스펙 적용
                if (DebuffEffectClass)
                {
                    FGameplayEffectSpecHandle DebuffSpecHandle = MakeOutgoingGameplayEffectSpec(DebuffEffectClass, GetAbilityLevel());
                    if (DebuffSpecHandle.IsValid() && DebuffSpecHandle.Data.IsValid())
                    {
                        FGameplayEffectContextHandle DebuffContext = DebuffSpecHandle.Data->GetContext();
                        DebuffContext.AddSourceObject(Character);
                        DebuffContext.AddInstigator(Character, Character);
                        DebuffSpecHandle.Data->SetContext(DebuffContext.Duplicate());
                        
                        Projectile->AdditionalEffectSpecHandles.Add(DebuffSpecHandle);
                    }
                }

                Projectile->FinishSpawning(SpawnTransform);
            }
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