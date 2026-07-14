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
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h" // 💡 [데이터 에셋 참조용 헤더 주입]

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
    
    // 💡 [기획 명세 1단계: 데이터 에셋 공격력 실시간 연동 필터]
    // 기본 어빌리티의 BaseDamage 값을 유지하되, 미니언 캐릭터 정보와 기획 데이터 자산 장부가 살아있다면
    // 에디터에서 자유롭게 깎고 늘린 'DefaultAttackPower' 기획 수치로 대미지를 덮어씌웁니다.
    float ActualDamage = BaseDamage;
    AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
    if (MinionChar && MinionChar->GetMinionData())
    {
        ActualDamage = MinionChar->GetMinionData()->DefaultAttackPower;
    }

    if (NewSpecHandle.IsValid())
    {
        // 💡 2. 동적 인양에 성공한 무결한 대미지 정본 수치를 이펙트 장부에 최종 주입합니다.
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, ActualDamage);
    }
    
    // 미니언 소체 장부로부터 데이터 에셋 기반의 투사체 클래스 동적 인양
    TSubclassOf<AFT_ProjectileBase> TargetProjectileClass = nullptr;
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
            // 💡 [명치 기준 수평 보정 사출 파이프라인 기동]
            // 1. 소켓을 무시하고 강제로 캐릭터 명치(가슴) 높이 좌표를 계산합니다.
            FVector ChestLocation = AvatarChar->GetActorLocation() + FVector(0.f, 0.f, 50.f); 
        
            // 2. 캐릭터 몸 안에 총알이 스폰되어 자폭하지 않도록, 앞으로 50만큼 밀어줍니다.
            FVector SpawnLocation = ChestLocation + (AvatarChar->GetActorForwardVector() * 50.f);

            // 3. 조준점(타깃)도 나와 완벽히 똑같은 수평(Z) 높이로 락온시켜서 일직선으로 날아가게 합니다.
            FVector TargetCenterLocation = TargetActor->GetActorLocation();
            TargetCenterLocation.Z = SpawnLocation.Z; 
        
            // 4. 방향과 트랜스폼 최종 계산
            FVector LaunchDirection = (TargetCenterLocation - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
        
            // 사출 트랜스폼 회전축 명시적 FQuat 형변환 및 대입
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