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
    // 각 개체별 독립적인 전술 타이머 제어를 위해 인스턴싱 정책을 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 기절 상태이상 노출 시 최초 활성화를 원천 차단하는 가드 태그를 설정합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
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
        // 0.1에서 0.2초 주기의 정밀 전술 로직 틱 타이머를 스타트합니다.
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
    
    // 안전 가드선 수선: 사망 장부 검적 시 유령 타이머가 죽은 액터를 재참조하여 크래시를 내지 않도록 타이머를 선제 클리어합니다.
    if (!MyASC || MyASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
        }
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 기절 상태이상 버프/디버프에 노출되었을 경우 무브먼트 물리 가속까지 완전 동결 처리합니다.
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
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    if (!AIC) return;

    UBlackboardComponent* BB = AIC->GetBlackboardComponent();
    if (!BB) return;

    UPathFollowingComponent* PathFollowComp = AIC->GetPathFollowingComponent();
    AActor* BestTarget = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetEnemy")));

    if (IsValid(BestTarget))
    {
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            
            // 사거리 진입 시 일반 공격 능력을 실행합니다.
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // 최적화 가드벽: 추격 무빙 리패싱 맹점 정밀 완치
            // 내비게이션 컴포넌트가 현재 추격 중인 실제 패스 최종 목적지와 타깃의 실시간 좌표를 오차 범위 내로 대조합니다.
            bool bAlreadyMovingToTarget = false;
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                FVector TargetLoc = BestTarget->GetActorLocation();

                // 타깃이 이동 틱 오차 범위(30cm) 내에 여전히 머물러 있다면 불필요한 중복 Move 명령을 청정 스킵합니다.
                if (CurrentDest.Equals(TargetLoc, 30.0f))
                {
                    bAlreadyMovingToTarget = true;
                }
            }

            if (!bAlreadyMovingToTarget)
            {
                AIC->MoveToActor(BestTarget, AttackAcceptanceRadius, true, true, true);
            }
        }
    }
    else
    {
        // 주위에 위협이 없다면 타깃 락 클리어 후 라인 복귀 연산을 단행합니다.
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
        if (MinionChar)
        {
            AFT_WayPoint* TargetWP = MinionChar->GetCurrentTargetWayPoint();
            if (TargetWP)
            {
                float DistanceToWP = FVector::Dist(MinionChar->GetActorLocation(), TargetWP->GetActorLocation());
                
                // 웨이포인트 도달 반경 검증을 통과하면 다음 거점으로 경로를 전진 이관합니다.
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
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
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
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    // 사망 및 종료 시 브레인 정화 완착
    // 미니언이 사망하거나 브레인이 종료될 때 잡고 있던 조준(Focus) 상태를 강제 해제하여 시체 시선 고정 버그를 청정 소각합니다.
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