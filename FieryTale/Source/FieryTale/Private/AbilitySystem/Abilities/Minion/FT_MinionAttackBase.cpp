// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_MinionAttackBase.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "AIController.h"
#include "AbilitySystemInterface.h" 
#include "Object/FT_ProjectileBase.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UFT_MinionAttackBase::UFT_MinionAttackBase()
{
    // 인스턴싱 정책: 미니언 군단의 수많은 개체별로 런타임 공격 데이터를 독립 제어하기 위해 인스턴스화
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 상태이상 봉쇄 태그: 기절(Stunned) 디버프 상태 레이어가 걸려있을 경우 미니언의 공격 행동을 엔진 단에서 원천 차단
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

void UFT_MinionAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [GAS 순정 시전 검증 관문]: 비용 및 제한 사항 체크 후 부적합 시 탈출
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
    // 미니언 고유의 공격 모션 애니메이션 시동을 처리합니다.
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, TEXT("MinionAttackTask"), AttackMontage, 1.0f, NAME_None, false
    );

    if (MontageTask)
    {
        // 애니메이션 수명 주기 완료 및 취소 상태를 추적할 마감 콜백 바인딩
        MontageTask->OnCompleted.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
        MontageTask->OnBlendOut.AddDynamic(this, &UFT_MinionAttackBase::OnMontageCompletedOrCancelled);
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
    // 애니메이션이 무작정 사출을 격발하던 구형 방식을 파쇄하고, 몽타주 내부 트랙에서
    // 기획자가 설정한 사출 타이밍 노티파이(Minion_Attack) 태그를 정확히 포착 대기합니다.
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, 
        FTTags::FTAbilities::Minion_Attack,
        nullptr, 
        false, 
        false
    );

    if (WaitEventTask)
    {
        // 몽타주 내부에서 해당 노티파이 싱크가 터지는 시점에 온전하게 아래의 사출 연산 함수를 동기 격발합니다.
        WaitEventTask->EventReceived.AddDynamic(this, &UFT_MinionAttackBase::OnMontageTargetedEvent);
        WaitEventTask->ReadyForActivation();
    }
}

void UFT_MinionAttackBase::OnMontageTargetedEvent(FGameplayEventData EventData)
{
    // [애니메이션 노티파이 프레임 동기화 격발 구역]
    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    
    if (!AIC || !MyASC || !DamageEffectClass) return;

    // AI 컨트롤러가 시선 락온을 고정하고 있는 현재 어그로 타깃 확보
    AActor* TargetActor = AIC->GetFocusActor();
    if (!TargetActor) return;

    // GAS 화력 계산서 발행: 적중 및 폭발 시 대상의 체력을 감산시킬 이펙트 명세 생성
    FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
    EffectContext.AddSourceObject(AvatarChar);
    FGameplayEffectSpecHandle NewSpecHandle = MyASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
    
    if (NewSpecHandle.IsValid())
    {
        // 기획 깡데미지 수치 명세를 계산서 우체통에 정밀 주입
        NewSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    }
    
    // 무기 형태 분기 라인 가동
    if (ProjectileClass)
    {
        // [원거리 미니언 분기]: 노티파이가 밟힌 바로 그 순간, 정밀 마법 투사체 지연 사출 가동
        UWorld* World = GetWorld();
        if (World)
        {
            // 좌표 연산: 미니언 가슴/눈높이 위치인 지상 50cm 원점 산출 후 타깃 방향 벡터 락온
            FVector SpawnLocation = AvatarChar->GetActorLocation() + FVector(0, 0, 50);
            FVector LaunchDirection = (TargetActor->GetActorLocation() - SpawnLocation).GetSafeNormal();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                ProjectileClass, SpawnTransform, AvatarChar, AvatarChar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                // 데이터 조립 완공된 대미지 명세 패킷을 투사체 배관망에 완착 토스
                Projectile->DamageEffectSpecHandle = NewSpecHandle;
                
                // 최종 월드 실물화 격발 방출
                Projectile->FinishSpawning(SpawnTransform);
            }
        }
    }
    else
    {
        // [근접 미니언 분기]: 무기를 내지른 타격 프레임 시점에 즉각적으로 인스턴트 대미지 다이렉트 주입
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(TargetActor))
        {
            if (UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent())
            {
                if (NewSpecHandle.IsValid())
                {
                    MyASC->ApplyGameplayEffectSpecToTarget(*NewSpecHandle.Data.Get(), TargetASC);
                }
            }
        }
    }
}

void UFT_MinionAttackBase::OnMontageCompletedOrCancelled()
{
    // 평타 사격 모션 완결 또는 CC기 피격에 의해 인터럽트 발생 시 즉시 장부 정리 관문으로 이동
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_MinionAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 마스터 상위 클래스로 소유 수명 주기를 넘겨주며 미니언 액션 입력 잠금 최종 해제 복구
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}