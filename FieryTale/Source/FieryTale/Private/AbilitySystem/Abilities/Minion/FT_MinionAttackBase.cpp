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
    // 인스턴싱 정책: 미니언 개체별 개별 쿨타임 및 공격 상태 제어를 위해 인스턴스 격리 생성
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 마스터 CC 가드선 완착]
    // 기절(Stunned) 디버프 상태 레이어가 가동 중일 때는 미니언 공격 행동 자체를 차단 봉쇄합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

void UFT_MinionAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar || !AttackMontage)
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // 1단계: 순정 애니메이션 몽타주 재생 태스크 가동
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, TEXT("MinionAttackTask"), AttackMontage, 1.0f, NAME_None, false
    );

    if (MontageTask)
    {
        // [타이밍 릭 수선]: OnBlendOut 관문을 제외시킵니다. 블렌드가 시작되어도 몽타주가 완전히 끝날 때(OnCompleted)까지 
        // 어빌리티 생명주기를 살려두어 노티파이 이벤트 가 사출 전 공중 분해되는 버그를 원천 진압합니다.
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        
        MontageTask->ReadyForActivation();
    }
    else
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // 2단계: 게임플레이 이벤트 수신 대기 태스크 병렬 가동
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, 
        FTTags::FTAbilities::Minion_Attack,
        nullptr, 
        false, 
        false
    );

    if (WaitEventTask)
    {
        WaitEventTask->EventReceived.AddDynamic(this, &UFT_MinionAttackBase::OnMontageTargetedEvent);
        WaitEventTask->ReadyForActivation();
    }
}

void UFT_MinionAttackBase::OnMontageTargetedEvent(FGameplayEventData EventData)
{
    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    
    if (!AIC || !MyASC || !DamageEffectClass) return;

    // 브레인 어빌리티가 실시간으로 고정해 준 Focus 대상(적 영웅, 적 미니언, 구조물)을 타깃 액터로 정밀 견인합니다.
    AActor* TargetActor = AIC->GetFocusActor();
    if (!TargetActor) return;

    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    if (NewSpecHandle.IsValid())
    {
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    }
    
    // 무기 형태 분기 라인 가동
    if (ProjectileClass)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            FVector SpawnLocation = AvatarChar->GetActorLocation() + FVector(0, 0, 50);
            FVector LaunchDirection = (TargetActor->GetActorLocation() - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            // [순정 투사체 사출]: 투사체가 자체적으로 Instigator(AvatarChar)를 파싱해 
            // 피아식별 팀 마크를 자율 식별하므로, 어빌리티 단은 대미지만 얹어서 깔끔하게 방출합니다.
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
        // [근접 미니언 분기]: 포탑/구조물까지 무결하게 포괄 타격하는 라이브러리 안전 배관 유지
        if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
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
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}