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

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 상태 이상(Stun) 상태일 때는 해당 어빌리티의 활성화를 원천 차단
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

    // 1단계: 공격 애니메이션 몽타주 재생 태스크 생성 및 바인딩
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

    // 2단계: 몽타주 노티파이 시점에 연동될 게임플레이 이벤트 수신 태스크 가동
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
    
    // 데이터 에셋 누수 등으로 인한 어빌리티 영구 프리징 방어를 위한 선제 가드
    if (!AIC || !MyASC || !DamageEffectClass)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // AI 포커스 대상 검증 (Dangling Pointer 및 유효성 2중 검문)
    AActor* TargetActor = AIC->GetFocusActor();
    if (!IsValid(TargetActor)) return; 

    // 타깃이 이미 사망 상태인 경우 불필요한 데미지 계산 및 투사체 스폰 스킵
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC && TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        return;
    }

    // 데미지 GameplayEffect Spec 설정 및 수치 기입
    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    if (NewSpecHandle.IsValid())
    {
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    }
    
    // 미니언 소체(CharacterBase) 장부로부터 데이터 에셋 기반의 투사체 클래스 동적 인양
    TSubclassOf<AFT_ProjectileBase> TargetProjectileClass = nullptr;
    if (AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar))
    {
        TargetProjectileClass = MinionChar->GetMinionProjectileClass();
    }
    
    // [원거리 무기 형태 분기]: 투사체 클래스가 유효할 경우
    if (TargetProjectileClass)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            USkeletalMeshComponent* MinionMesh = AvatarChar->GetMesh();
            
            // 기본 스폰 트랜스폼 데이터 초기화 (소켓 예외 가드용)
            FVector SpawnLocation = AvatarChar->GetActorLocation() + FVector(0, 0, 50);
            FRotator SpawnRotation = AvatarChar->GetActorRotation();

            // 에디터 스켈레톤의 우손 소총 소켓 존재 여부 정밀 검문 및 위치 동기화
            FName MuzzleSocketName = TEXT("Socket_R_Hand_Riple"); 
            if (MinionMesh && MinionMesh->DoesSocketExist(MuzzleSocketName))
            {
                SpawnLocation = MinionMesh->GetSocketLocation(MuzzleSocketName);
            }
            
            // 타깃 캐릭터의 가슴/상체 권역을 타겟팅하도록 보정 연산
            FVector TargetCenterLocation = TargetActor->GetActorLocation() + FVector(0, 0, 30);
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            // 지연 생성(Deferred Spawn)을 통한 투사체 인스턴스 사출 및 스펙 데이터 링크 결착
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
        // [근접 무기 형태 분기]: 투사체가 없을 경우 타깃에 GameplayEffect 다이렉트 적용
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
    // 어빌리티 종료 시 상주 중인 비동기 WaitGameplayEvent 태스크 명시적 클리어 (메모리 누수 차단)
    if (ActiveWaitEventTask)
    {
        ActiveWaitEventTask->EndTask();
        ActiveWaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}