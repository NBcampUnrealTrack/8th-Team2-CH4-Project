// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Minion/FT_MinionAttackBase.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "AIController.h"
#include "Object/FT_ProjectileBase.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 기절 상태 이상 레이어 가동 중일 때 공격 활성화 원천 차단
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
    if (!AvatarChar || !AttackMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1단계: 순정 애니메이션 몽타주 재생 태스크 가동
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, TEXT("MinionAttackTask"), AttackMontage, 1.0f, NAME_None, false
    );

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        
        MontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2단계: 게임플레이 이벤트 수신 대기 태스크 병렬 가동
    // 💡 [좀비 태스크 방어선]: 안전한 소각 청소를 위해 로컬 변수가 아닌 헤더에 선언된 
    // 멤버 변수(ActiveWaitEventTask)에 주소를 확실히 결착합니다.
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
    if (!AvatarChar) return;

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    
    if (!AIC || !MyASC || !DamageEffectClass) return;

    // 브레인 포커스 대상 인양 후 즉시 댕글링 포인터 검문
    AActor* TargetActor = AIC->GetFocusActor();
    if (!IsValid(TargetActor)) return; // 💡 IsValid를 통해 메모리 파괴 여부까지 2중 방어

    // [오버킬/타깃 유효성 가드선]
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC && TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        return;
    }

    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    if (NewSpecHandle.IsValid())
    {
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    }
    
    // 원거리 무기 형태 분기 라인 가동
    if (ProjectileClass)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            FVector SpawnLocation = AvatarChar->GetActorLocation() + FVector(0, 0, 50);
            
            // 💡 [액세스 위반 크래시 완전 완치]: 검증이 완료된 TargetActor 명세를 안전선 타설 후 인양합니다.
            FVector TargetCenterLocation = TargetActor->GetActorLocation() + FVector(0, 0, 50);
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                ProjectileClass, SpawnTransform, AvatarChar, AvatarChar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                Projectile->DamageEffectSpecHandle = NewSpecHandle;
                Projectile->FinishSpawning(SpawnTransform);
            }
        }
    }
    else
    {
        // 근접 미니언 분기
        if (TargetASC)
        {
            if (NewSpecHandle.IsValid())
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
    // 💡 [태스크 메모리 누수 청정 소각]:
    // 어빌리티가 종료될 때 상주 중이던 비동기 이벤트 태스크를 명시적으로 파쇄 마감합니다.
    if (ActiveWaitEventTask)
    {
        ActiveWaitEventTask->EndTask();
        ActiveWaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}