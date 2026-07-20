// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Minion/FT_MinionAttackBase.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "AIController.h"
#include "Object/FT_ProjectileBase.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 상태 이상 시 어빌리티 활성화를 차단합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

void UFT_MinionAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // 공격 몽타주가 존재할 경우 재생 태스크를 실행합니다.
    UAbilityTask_PlayMontageAndWait* MontageTask = nullptr;
    UAnimMontage* LoadedMontage = nullptr;
    if (!AttackMontage.IsNull())
    {
        LoadedMontage = AttackMontage.LoadSynchronous();
    }
    
    if (LoadedMontage != nullptr)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("MinionAttackTask"), LoadedMontage, 1.0f, NAME_None, false
        );
    }

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        
        MontageTask->ReadyForActivation();
    }
    // 노티파이(Notify) 설정 누락으로 인한 공격 씹힘 방지:
    // 플레이어 평타(NormalAttack)처럼 어빌리티 실행 시점에 즉각적으로(혹은 약간의 선딜레이 후) 공격을 강제 판정합니다.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(DebugNoMontageTimerHandle, [this]()
        {
            FGameplayEventData DummyEventData;
            OnMontageTargetedEvent(DummyEventData);
        }, 0.25f, false); // 애니메이션 싱크를 위해 0.25초 후 발사 (필요 시 0.0f로 즉시 발사 가능)
    }
}

void UFT_MinionAttackBase::OnMontageTargetedEvent(FGameplayEventData EventData)
{
    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    
    // 서버에서만 처리합니다.
    if (!AvatarChar || !AvatarChar->HasAuthority())
    {
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    
    if (!AIC || !MyASC || !DamageEffectClass)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // AI 타깃 검증
    AActor* TargetActor = AIC->GetFocusActor();
    if (!IsValid(TargetActor))
    {
        return;
    }
    // 사망한 타깃 혹은 무력화된 타워 제외
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC && (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead) ||
                      TargetASC->HasMatchingGameplayTag(FTTags::FTCombat::Structure_Muted)))
    {
        return;
    }

    // 대미지 이펙트 스펙 생성
    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    // 데이터 에셋에 정의된 피해량을 적용합니다.
    float ActualDamage = BaseDamage;
    AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
    if (MinionChar && MinionChar->GetMinionData())
    {
        ActualDamage = MinionChar->GetMinionData()->DefaultAttackPower;
    }

    if (NewSpecHandle.IsValid())
    {
        // 피해량 설정
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, ActualDamage);
    }
    
    // 투사체 클래스 획득
    TSoftClassPtr<AFT_ProjectileBase> TargetProjectileClass = ProjectileClass; // 어빌리티 자체 설정값을 기본으로 사용
    if (MinionChar && !MinionChar->GetMinionProjectileClass().IsNull())
    {
        // 미니언 데이터에 투사체가 명시되어 있다면 그걸로 덮어씌움 (우선순위)
        TargetProjectileClass = MinionChar->GetMinionProjectileClass();
    }
    
    // 투사체를 스폰합니다.
    if (!TargetProjectileClass.IsNull())
    {
        UClass* LoadedProjectileClass = TargetProjectileClass.LoadSynchronous();
        UWorld* World = GetWorld();
        if (World && LoadedProjectileClass)
        {
            // 스폰 위치 계산
            // 스폰 위치 계산 (미니언 전용 무기 소켓 추적 파이프라인 활용)
            FVector SpawnLocation;
            if (MinionChar)
            {
                SpawnLocation = MinionChar->GetAttackLaunchTransform(TEXT("R_Hand_FirePoint")).GetLocation();
            }
            else
            {
                // Fallback for non-minions (though this ability is minion-only)
                SpawnLocation = AvatarChar->GetActorLocation() + FVector(0.f, 0.f, 50.f) + (AvatarChar->GetActorForwardVector() * 50.f);
            }

            // 타격점 높이 동기화 (투사체가 위/아래로 너무 쏠리지 않도록, 소켓 출발 높이 기준으로 유지)
            FVector TargetCenterLocation = TargetActor->GetActorLocation();
            TargetCenterLocation.Z = SpawnLocation.Z; 
        
            // 스폰 트랜스폼 계산
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
        
            SpawnTransform.SetRotation(FQuat(LaunchDirection.Rotation()));

            // 서버에서 투사체 스폰
            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                LoadedProjectileClass, SpawnTransform, AvatarChar, AvatarChar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                Projectile->DamageEffectSpecHandle = NewSpecHandle;
                Projectile->FinishSpawning(SpawnTransform);
            }
        }
    }
    else
    {
        // 투사체가 없으면 근접 공격으로 판정하여 대상에게 대미지 적용
        if (TargetASC && NewSpecHandle.IsValid())
        {
            if (NewSpecHandle.Data.IsValid())
            {
                MyASC->ApplyGameplayEffectSpecToTarget(*NewSpecHandle.Data.Get(), TargetASC);
            }
        }
    }
}

void UFT_MinionAttackBase::OnMontageCompletedOrCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_MinionAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 디버그 타이머 해제
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DebugNoMontageTimerHandle);
    }

    if (ActiveWaitEventTask)
    {
        ActiveWaitEventTask->EndTask();
        ActiveWaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}