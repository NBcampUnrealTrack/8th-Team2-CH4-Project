// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_Minion_Brain.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "Character/Minion/FT_MinionAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "AbilitySystemBlueprintLibrary.h"

// AbilitySystem/Abilities/Minion/FT_Minion_Brain.cpp

UFT_Minion_Brain::UFT_Minion_Brain()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
    bWasStunnedLastTick = false;
    
    ScanInterval = 0.25f; 
    AttackAcceptanceRadius = 150.0f; // 평타 거리 안전 규격 기본값 확보
}

void UFT_Minion_Brain::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    if (GetWorld())
    {
        // 💡 [런타임 세이프티 가드]: 혹여나 블루프린트 디테일 패널에서 실수로 0.0f 이하로 밀어버렸다면,
        // 타이머가 엔진에서 터지거나 파괴되는 것을 막기 위해 하한 마진(0.1초)으로 강제 스냅 보정합니다.
        if (ScanInterval <= 0.0f)
        {
            ScanInterval = 0.25f;
        }

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

    AFT_MinionAIController* AIC = Cast<AFT_MinionAIController>(AvatarChar->GetController());
    if (!AIC) return;

    if (MyASC->HasMatchingGameplayTag(FTTags::FTStates::Debuff::Stunned))
    {
        AIC->StopMovement();
        if (UCharacterMovementComponent* MoveComp = AvatarChar->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately();
        }
        bWasStunnedLastTick = true;
        return;
    }

    UPathFollowingComponent* PathFollowComp = AIC->GetPathFollowingComponent();
    AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
    
    // =========================================================================
    // 💡 [실시간 생존 타깃 스캔 엔진 개통] 
    // 컨트롤러의 죽은 수동 락온 장부를 제거하고, 퍼셉션 컴포넌트 목록을 실시간 역산 스캔합니다.
    // =========================================================================
    AActor* BestTarget = nullptr;
    
    if (AIC->GetPerceptionComponent() && MinionChar)
    {
        TArray<AActor*> PerceivedActors;
        AIC->GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

        float ClosestDistanceSq = TNumericLimits<float>::Max();
        FVector MyLoc = AvatarChar->GetActorLocation();

        for (AActor* PerceivedActor : PerceivedActors)
        {
            if (!IsValid(PerceivedActor) || PerceivedActor == AvatarChar) continue;

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PerceivedActor);
            if (!TargetASC) continue;

            // 1차 검문: 타깃이 이미 사망 상태(Dead)라면 시체 구타 방지를 위해 즉각 소각 스킵
            if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
            {
                continue;
            }

            // 2차 검문: 나와 다른 적 진영 태그를 지니고 있는지 식별 (소체 내부의 적 진영 판정 태그 규칙 활용)
            bool bIsEnemy = false;
            if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
            {
                bIsEnemy = true;
            }
            else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
            {
                bIsEnemy = true;
            }

            // 3차 검문: 적 진영이 확정되었다면 실시간 최단 거리 미니언 락온 갱신
            if (bIsEnemy)
            {
                float DistSq = FVector::DistSquared(MyLoc, PerceivedActor->GetActorLocation());
                if (DistSq < ClosestDistanceSq)
                {
                    ClosestDistanceSq = DistSq;
                    BestTarget = PerceivedActor;
                }
            }
        }
    }

    // [전술 상황 1: 유효 생존 타깃 검출 시 추격 및 교전]
    if (IsValid(BestTarget))
    {
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            bool bAlreadyMovingToTarget = false;
            
            // 💡 [순정 API 표준 결착]: GetPathGoalActor -> GetMoveGoal()로 교체하여 심볼 에러를 완전히 박멸합니다.
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving && !bWasStunnedLastTick)
            {
                // 패스파인더가 현재 실시간 추적(Goal Tracking) 중인 액터 주소 직격 인양
                if (PathFollowComp->GetMoveGoal() == BestTarget)
                {
                    bAlreadyMovingToTarget = true;
                }
            }

            if (!bAlreadyMovingToTarget)
            {
                // Goal Tracking 아키텍처로 안전 사출
                FAIMoveRequest MoveRequest;
                MoveRequest.SetGoalActor(BestTarget);
                MoveRequest.SetAcceptanceRadius(AttackAcceptanceRadius);
                MoveRequest.SetUsePathfinding(true);
                MoveRequest.SetCanStrafe(true);
                MoveRequest.SetAllowPartialPath(true);

                AIC->MoveTo(MoveRequest);
                bWasStunnedLastTick = false; 
            }
        }
    }
    // [전술 상황 2: 적대 타깃 부재 혹은 사망 시 라인 웨이포인트 복귀 무빙]
    else
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        bool bWaypointChanged = false;
        FVector NextLineLocation = FVector::ZeroVector;

        if (MinionChar)
        {
            AFT_WayPoint* TargetWP = MinionChar->GetCurrentTargetWayPoint();
            if (TargetWP)
            {
                float DistanceToWP = FVector::Dist(MinionChar->GetActorLocation(), TargetWP->GetActorLocation());
                
                if (DistanceToWP <= TargetWP->ArrivalRadius)
                {
                    AFT_WayPoint* NextWP = TargetWP->NextWayPoint;
                    MinionChar->SetCurrentTargetWayPoint(NextWP);
                    TargetWP = NextWP;
                    bWaypointChanged = true;
                }
                
                if (TargetWP)
                {
                    NextLineLocation = TargetWP->GetActorLocation();
                }
            }
        }

        if (!NextLineLocation.IsZero())
        {
            bool bAlreadyMovingToLoc = false;
            
            if (bWaypointChanged)
            {
                AIC->StopMovement();
            }
            else if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                if (CurrentDest.Equals(NextLineLocation, 60.0f)) // 도달 반경과 동기화하여 허용 범위 여유 보정
                {
                    bAlreadyMovingToLoc = true;
                }
            }

            if (!bAlreadyMovingToLoc)
            {
                FAIMoveRequest MoveRequest;
                MoveRequest.SetGoalLocation(NextLineLocation);
                MoveRequest.SetAcceptanceRadius(60.0f);
                MoveRequest.SetUsePathfinding(true);
                MoveRequest.SetCanStrafe(true);
                MoveRequest.SetAllowPartialPath(true); 
                
                FPathFollowingRequestResult Result = AIC->MoveTo(MoveRequest);
                
                if (Result.Code == EPathFollowingRequestResult::Failed)
                {
                    FVector Direction = (NextLineLocation - AvatarChar->GetActorLocation()).GetSafeNormal2D();
                    AvatarChar->AddMovementInput(Direction, 1.0f, true);
                }
            }
        }
        
        bWasStunnedLastTick = false;
    }
}

AActor* UFT_Minion_Brain::InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag)
{
    return nullptr;
}

void UFT_Minion_Brain::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

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