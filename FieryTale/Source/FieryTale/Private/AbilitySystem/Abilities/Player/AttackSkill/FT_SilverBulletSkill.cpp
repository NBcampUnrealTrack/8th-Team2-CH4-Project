// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
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
    
    // 장전 몽타주 재생 태스크 실행
    UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("SilverBulletChannellingTask"), LoadedMontage, 1.0f);

        if (MontageTask)
        {
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
            MontageTask->ReadyForActivation();
        }
    }

    // 장전 타이머 설정
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

        if (Character && MyASC)
        {
            UWorld* World = GetWorld();
            
            // 투사체 에셋을 비동기(또는 동기) 로드하여 메모리에 적재합니다.
            UClass* LoadedProjectileClass = ProjectileClass.LoadSynchronous();

            // 투사체를 스폰합니다.
            if (World && LoadedProjectileClass && DamageEffectClass)
            {
                // 스폰 위치 및 방향 계산
                FVector SpawnLocation = Character->GetWeaponMuzzleLocation();
                // 액터 정면 대신 카메라 조준선의 첫 충돌 지점을 향하도록 발사 방향을 보정한다.
                FVector LaunchDirection = Character->GetCameraAimDirection(SpawnLocation);

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
                SpawnTransform.SetRotation(FQuat(LaunchDirection.Rotation()));

                // 대미지 이펙트 스펙 생성
                FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
                if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
                {
                    FGameplayEffectContextHandle SkillContext = SpecHandle.Data->GetContext();
                    SkillContext.AddSourceObject(Character);
                    SkillContext.AddInstigator(Character, Character);
                    
                    SpecHandle.Data->SetContext(SkillContext.Duplicate());
                    SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                }

                // 서버에서 투사체 스폰을 처리합니다.
                if (Character->HasAuthority())
                {
                    AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                        LoadedProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
                    if (Projectile)
                    {
                        Projectile->DamageEffectSpecHandle = SpecHandle;
                        
                        // 슬로우 이펙트 스펙 추가
                        if (SlowEffectClass)
                        {
                            FGameplayEffectSpecHandle SlowSpecHandle = MakeOutgoingGameplayEffectSpec(SlowEffectClass, GetAbilityLevel());
                            if (SlowSpecHandle.IsValid() && SlowSpecHandle.Data.IsValid())
                            {
                                FGameplayEffectContextHandle SlowContext = SlowSpecHandle.Data->GetContext();
                                SlowContext.AddSourceObject(Character);
                                SlowContext.AddInstigator(Character, Character);
                                SlowSpecHandle.Data->SetContext(SlowContext.Duplicate());
                                
                                Projectile->AdditionalEffectSpecHandles.Add(SlowSpecHandle);
                            }
                        }

                        Projectile->FinishSpawning(SpawnTransform);
                    }
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