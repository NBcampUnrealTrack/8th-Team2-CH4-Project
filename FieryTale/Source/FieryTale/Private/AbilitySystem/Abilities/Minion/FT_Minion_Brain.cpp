// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Minion/FT_Minion_Brain.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"

UFT_Minion_Brain::UFT_Minion_Brain()
{
    // 개체별 독립적인 주기적 연산을 위해 인스턴싱 정책 설정
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 기절(Stun) 상태이상 적용 중일 때는 브레인 활성화를 원천 차단
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
    
    // 스턴 상태 추적 플래그 초기화
    bWasStunnedLastTick = false;
}

void UFT_Minion_Brain::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // 지정된 주기(ScanInterval)마다 AI 판단 로직을 격발하는 타이머 가동
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AIExecuteTimerHandle,
            this,
            &UFT_Minion_Brain::ExecuteAILogic,
            ScanInterval,
            true
        );
    }
}

void UFT_Minion_Brain::ExecuteAILogic()
{
    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    
    // 액터 사망 상태 확인 시, 월드 참조 오류 방지를 위해 타이머를 우선 클리어하고 종료
    if (!MyASC || MyASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        UWorld* World = GetWorld();
        if (World)
        {
            World->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
        }
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 기절 제어 상태 이상 적용 시 즉시 무브먼트 컴포넌트 및 AI 패스 이동 동결
    if (MyASC->HasMatchingGameplayTag(FTTags::FTStates::Debuff::Stunned))
    {
        if (AAIController* AIC = Cast<AAIController>(AvatarChar->GetController()))
        {
            AIC->StopMovement();
        }
        if (UCharacterMovementComponent* MoveComp = AvatarChar->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately();
        }
        
        // 스턴 상태 해제 시점의 재추격을 유도하기 위한 상태 캐싱
        bWasStunnedLastTick = true;
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    if (!AIC) return;

    UBlackboardComponent* BB = AIC->GetBlackboardComponent();
    if (!BB) return;

    UPathFollowingComponent* PathFollowComp = AIC->GetPathFollowingComponent();
    AActor* BestTarget = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetEnemy")));

    // [전술 상황 1: 유효 타깃 검출 시 추격 및 교전]
    if (IsValid(BestTarget))
    {
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            // 공격 사거리 내 진입 시 이동을 멈추고 공격 태그 어빌리티 격발
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // 이동 목적지 중복 명령 최적화 및 스턴 해제 직후 프리징 복구 분기
            bool bAlreadyMovingToTarget = false;
            
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving && !bWasStunnedLastTick)
            {
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                FVector TargetLoc = BestTarget->GetActorLocation();

                // 기존 목적지와 대상의 실시간 위치 오차가 30cm 이내면 재명령 스킵
                if (CurrentDest.Equals(TargetLoc, 30.0f))
                {
                    bAlreadyMovingToTarget = true;
                }
            }

            // 스턴이 풀렸거나 목적지가 변경된 경우 이동 명령 갱신
            if (!bAlreadyMovingToTarget)
            {
                AIC->MoveToActor(BestTarget, AttackAcceptanceRadius, true, true, true);
                bWasStunnedLastTick = false; 
            }
        }
    }
    // [전술 상황 2: 적대 타깃 부재 시 라인 웨이포인트 복귀 무빙]
    else
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
        if (MinionChar)
        {
            AFT_WayPoint* TargetWP = MinionChar->GetCurrentTargetWayPoint();
            if (TargetWP)
            {
                float DistanceToWP = FVector::Dist(MinionChar->GetActorLocation(), TargetWP->GetActorLocation());
                
                // 웨이포인트 도달 반경 검증 통과 시 다음 목적지 거점으로 자동 갱신 및 인양
                if (DistanceToWP <= TargetWP->ArrivalRadius)
                {
                    AFT_WayPoint* NextWP = TargetWP->NextWayPoint;
                    MinionChar->SetCurrentTargetWayPoint(NextWP);
                    TargetWP = NextWP;
                }
                
                if (TargetWP)
                {
                    BB->SetValueAsVector(TEXT("LineWaypoint"), TargetWP->GetActorLocation());
                }
                else
                {
                    BB->ClearValue(TEXT("LineWaypoint"));
                }
            }
        }

        FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
        if (!NextLineLocation.IsZero())
        {
            bool bAlreadyMovingToLoc = false;
            
            // 라인 복귀 무빙 중 스턴 해제 시 예외 프리징 방어 처리 포함
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving && !bWasStunnedLastTick)
            {
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                if (CurrentDest.Equals(NextLineLocation, 10.0f))
                {
                    bAlreadyMovingToLoc = true;
                }
            }

            if (!bAlreadyMovingToLoc)
            {
                AIC->MoveToLocation(NextLineLocation, 50.0f, true, true, true);
                bWasStunnedLastTick = false;
            }
        }
    }
}

AActor* UFT_Minion_Brain::InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag)
{
    return nullptr;
}

void UFT_Minion_Brain::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 어빌리티 종료 시 상주 중인 판단 루프 타이머 안전 소각
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    // 어빌리티 파쇄 및 미니언 사망 시 조준/이동 상태 강제 초기화
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            if (AAIController* AIC = Cast<AAIController>(AvatarChar->GetController()))
            {
                AIC->ClearFocus(EAIFocusPriority::Gameplay);
                AIC->StopMovement();
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}