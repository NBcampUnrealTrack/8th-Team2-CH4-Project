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
#include "NavigationSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Engine/OverlapResult.h"

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
    
    if (!AvatarChar)
    {
        return;
    }

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
    if (!AIC)
    {
        return;
    }

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
    
    if (MinionChar && MinionChar->GetMinionData())
    {
        AttackAcceptanceRadius = MinionChar->GetMinionData()->DefaultAttackRange;
    }
    
    AActor* BestTarget = nullptr;
    
    if (MinionChar)
    {
        TArray<AActor*> PotentialTargets;

        // 1. 기본 AI 시야 감지 (미니언, 플레이어 등)
        if (AIC->GetPerceptionComponent())
        {
            AIC->GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PotentialTargets);
        }

        // 2. 물리 반경 스캔 (시야 시스템이 놓치는 건물/포탑 강제 색인)
        if (UWorld* World = GetWorld())
        {
            TArray<FOverlapResult> OverlapResults;
            FCollisionShape ScanSphere = FCollisionShape::MakeSphere(1500.0f); // 15m 스캔
            FCollisionObjectQueryParams ObjectQueryParams;
            ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn); // 플레이어, 미니언
            ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic); // 포탑, 넥서스 (CollisionBox)
            ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic); // 혹시 모를 정적 건물

            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(AvatarChar);

            if (World->OverlapMultiByObjectType(OverlapResults, AvatarChar->GetActorLocation(), FQuat::Identity, ObjectQueryParams, ScanSphere, QueryParams))
            {
                for (const FOverlapResult& Result : OverlapResults)
                {
                    if (AActor* OverlappedActor = Result.GetActor())
                    {
                        PotentialTargets.AddUnique(OverlappedActor);
                    }
                }
            }
        }

        float ClosestDistanceSq = TNumericLimits<float>::Max();
        FVector MyLoc = AvatarChar->GetActorLocation();

        for (AActor* PerceivedActor : PotentialTargets)
        {
            if (!IsValid(PerceivedActor) || PerceivedActor == AvatarChar)
            {
                continue;
            }

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PerceivedActor);
            if (!TargetASC)
            {
                continue;
            }

            if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead) ||
                TargetASC->HasMatchingGameplayTag(FTTags::FTCombat::Structure_Muted))
            {
                continue;
            }

            bool bIsEnemy = false;
            if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
            {
                bIsEnemy = true;
            }
            else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
            {
                bIsEnemy = true;
            }

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

    // =========================================================================
    // 유효 타깃 발견 시 추격 및 교전
    // =========================================================================
    if (IsValid(BestTarget))
    {

        // 두 캐릭터 간의 캡슐 표면 거리를 계산하여 공격 가능 여부를 판단합니다. 
        // 위치 보간 오차를 감안하여 50.0f의 마진을 적용합니다.
        float DistanceToTarget = AvatarChar->GetDistanceTo(BestTarget);

        if (DistanceToTarget <= AttackAcceptanceRadius + 50.0f)
        {
            AIC->SetFocus(BestTarget);
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            bool bSuccess = MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            AIC->ClearFocus(EAIFocusPriority::Gameplay); // 이동 중 포커스 고정으로 인한 회전 버그 방지

            bool bAlreadyMovingToTarget = false;
            
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving && !bWasStunnedLastTick)
            {
                // 목적지가 Target의 위치와 충분히 가까운지 확인
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                if (FVector::DistSquared(CurrentDest, BestTarget->GetActorLocation()) < 10000.0f) // 100cm 이내
                {
                    bAlreadyMovingToTarget = true;
                }
            }

            if (!bAlreadyMovingToTarget)
            {
                FVector GoalLocation = BestTarget->GetActorLocation();

                // 🌟 핵심: 거대 건물은 네비메시에 구멍을 뚫으므로, 건물 중심 좌표가 길찾기 불가능한 구역일 수 있습니다.
                // 따라서 타깃의 중심점과 가장 가까운 '네비메시 위'의 합법적인 좌표를 찾아서 거기로 길을 찾습니다.
                UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
                if (NavSys)
                {
                    FNavLocation ProjectedLocation;
                    // 타깃 주변 반경 1000cm 내에서 가장 가까운 네비메시 좌표 탐색
                    if (NavSys->ProjectPointToNavigation(GoalLocation, ProjectedLocation, FVector(1000.f, 1000.f, 1000.f)))
                    {
                        GoalLocation = ProjectedLocation.Location;
                    }
                }

                FAIMoveRequest MoveRequest;
                MoveRequest.SetGoalLocation(GoalLocation);
                MoveRequest.SetAcceptanceRadius(AttackAcceptanceRadius);
                MoveRequest.SetUsePathfinding(true);
                MoveRequest.SetCanStrafe(true);
                MoveRequest.SetAllowPartialPath(true);

                FPathFollowingRequestResult MoveResult = AIC->MoveTo(MoveRequest);
                
                // 만약 여전히 길찾기가 즉시 실패했다면 강제로 그 방향으로 직진 압력을 가함
                if (MoveResult.Code == EPathFollowingRequestResult::Failed)
                {
                    FVector Direction = (GoalLocation - AvatarChar->GetActorLocation()).GetSafeNormal2D();
                    AvatarChar->AddMovementInput(Direction, 1.0f, true);
                }

                bWasStunnedLastTick = false; 
            }
        }
    }
    // 타깃이 없을 경우 다음 웨이포인트로 이동
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
                if (CurrentDest.Equals(NextLineLocation, 60.0f)) 
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