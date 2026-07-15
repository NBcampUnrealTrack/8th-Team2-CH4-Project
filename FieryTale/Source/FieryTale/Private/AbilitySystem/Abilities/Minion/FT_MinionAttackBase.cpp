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
    if (AttackMontage != nullptr)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("MinionAttackTask"), AttackMontage, 1.0f, NAME_None, false
        );
    }

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        
        MontageTask->ReadyForActivation();
    }
    // 몽타주가 없을 경우 가상 딜레이 후 공격을 실행합니다.
    else
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(DebugNoMontageTimerHandle, [this]()
            {
                // 지연 시간 만료 시 강제로 공격 판정 실행
                FGameplayEventData DummyEventData;
                OnMontageTargetedEvent(DummyEventData);
                
                // 이후 어빌리티 종료
                if (UWorld* InnerWorld = GetWorld())
                {
                    InnerWorld->GetTimerManager().SetTimer(DebugNoMontageTimerHandle, [this]()
                    {
                        OnMontageCompletedOrCancelled();
                    }, 0.2f, false);
                }
            }, 0.5f, false);
        }
        else
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
        return;
    }

    // 몽타주 노티파이 시점에 게임플레이 이벤트를 대기하는 태스크 실행
    ActiveWaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, 
        FTTags::FTCombat::AnimNotify_Attack, 
        nullptr, 
        false, 
        false
    );

    if (ActiveWaitEventTask)
    {
        ActiveWaitEventTask->EventReceived.AddDynamic(this, &UFT_MinionAttackBase::OnMontageTargetedEvent);
        ActiveWaitEventTask->ReadyForActivation();
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
    TSubclassOf<AFT_ProjectileBase> TargetProjectileClass = nullptr;
    if (MinionChar)
    {
        TargetProjectileClass = MinionChar->GetMinionProjectileClass();
    }
    
    // 투사체를 스폰합니다.
    if (TargetProjectileClass)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            // 스폰 위치 계산
            FVector ChestLocation = AvatarChar->GetActorLocation() + FVector(0.f, 0.f, 50.f); 
            FVector SpawnLocation = ChestLocation + (AvatarChar->GetActorForwardVector() * 50.f);

            // 타격점 높이 동기화
            FVector TargetCenterLocation = TargetActor->GetActorLocation();
            TargetCenterLocation.Z = SpawnLocation.Z; 
        
            // 스폰 트랜스폼 계산
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
        
            SpawnTransform.SetRotation(FQuat(LaunchDirection.Rotation()));

            // 서버에서 투사체 스폰
            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                TargetProjectileClass, SpawnTransform, AvatarChar, AvatarChar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
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