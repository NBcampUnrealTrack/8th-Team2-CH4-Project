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
#include "TimerManager.h" // 💡 [디버그 타이머 배관용 헤더 주입]

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
    if (!AvatarChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // 1단계: 공격 애니메이션 몽타주가 존재할 때만 정상적인 어빌리티 태스크 생성 파이프라인 기동
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
    // 💡 [4단계 명세: 몽타주 프리패스 디버깅 모드 돌파선 완착]
    // 에셋이 비어있어도 프리징을 원천 봉쇄하고 0.5초 가상 선딜레이 후 강제로 사격 노티파이 로직을 격발시킵니다.
    else
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(DebugNoMontageTimerHandle, [this]()
            {
                // 0.5초 경과 시점: 실제 타격/사출 연산 (OnMontageTargetedEvent) 강제 기폭
                FGameplayEventData DummyEventData;
                OnMontageTargetedEvent(DummyEventData);
                
                // 그로부터 0.2초 경과 시점: 어빌리티를 정순 종료하여 다음 쿨타임 루프 개통
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

    // 2단계: 몽타주 노티파이 시점에 연동될 게임플레이 이벤트 수신 태스크 가동 (몽타주가 있을 때만 활성화)
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
    
    // [최전방 서버 주권 방어선 완착]
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

    // AI 포커스 대상 검증
    AActor* TargetActor = AIC->GetFocusActor();
    if (!IsValid(TargetActor)) return; 

    // 타깃이 이미 사망 상태인 경우 스킵
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC && TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        return;
    }

    // 데미지 GameplayEffect Spec 설정
    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    if (NewSpecHandle.IsValid())
    {
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    }
    
    // 미니언 소체 장부로부터 데이터 에셋 기반의 투사체 클래스 동적 인양
    TSubclassOf<AFT_ProjectileBase> TargetProjectileClass = nullptr;
    AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
    if (MinionChar)
    {
        TargetProjectileClass = MinionChar->GetMinionProjectileClass();
    }
    
    // [원거리 무기 형태 분기]: 투사체 클래스가 유효할 경우
    if (TargetProjectileClass)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            // 💡 [무기 자체 소켓 파이어포인트 배관 완벽 직결]
            // 이전에 완성해 둔 GetAttackLaunchTransform을 호출하여, 무기 스태틱 메쉬의 "FirePoint" 소켓을 1순위로 뒤지고
            // 메쉬가 없거나 소켓이 누락되었다면 오른손 소켓("R_Hand_Equip")으로 안전 회수(Fallback) 동기화합니다!
            FTransform SpawnTransform;
            if (MinionChar)
            {
                SpawnTransform = MinionChar->GetAttackLaunchTransform(TEXT("R_Hand_Equip"));
            }
            else
            {
                SpawnTransform = FTransform(AvatarChar->GetActorRotation(), AvatarChar->GetActorLocation() + FVector(0.f, 0.f, 60.f));
            }

            FVector SpawnLocation = SpawnTransform.GetLocation();
            
            // 타깃 캐릭터의 가슴/상체 권역을 정밀 리드 타겟팅 조준 보정
            FVector TargetCenterLocation = TargetActor->GetActorLocation() + FVector(0, 0, 30);
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            
            // 사출 트랜스폼 회전축 갱신 재배정
            SpawnTransform.SetRotation(FQuat(LaunchDirection.Rotation()));

            // 오직 서버에서만 해당 투사체를 공식 스폰하고 클라이언트로 리플리케이션
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
    // 💡 [추가 명세: 좀비 디버그 타이머 확실하게 청소]
    // 어빌리티가 중간에 캔슬되거나 강제 종료될 때, 예약되어 있던 가상 선딜레이 타이머 밸브를 즉시 소각 박멸합니다.
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