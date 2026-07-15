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

    //	[예시 1] 시전 순간의 연출을 GameplayCue로 격발한다.
    //	GameplayCueManager가 이 태그(GameplayCue.Alice.SkillCast)의 Notify(GCN_Alice_SkillCast)를 소프트 로드해 재생한다.
    //	그 Notify가 참조하는 Niagara/사운드를 캐릭터 데이터의 PreloadAssets에 넣어두면, 아레나 시작 시 미리 로드되어
    //	최초 시전 시의 로딩 히치가 사라진다. (하드 참조와 달리 큐 에셋은 평소엔 지연 로드되므로 프리로드가 실효를 갖는다.)
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        FGameplayCueParameters CueParams;
        if (const AActor* Avatar = GetAvatarActorFromActorInfo())
        {
            CueParams.Location = Avatar->GetActorLocation();
        }
        ASC->ExecuteGameplayCue(GameplayCue::Alice_SkillCast, CueParams);
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

    // 투사체를 스폰합니다.
    if (World && ClockRabbitProjectileClass && DamageEffectClass && Character)
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
                ClockRabbitProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
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