// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_Minion_Brain.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"

UFT_Minion_Brain::UFT_Minion_Brain()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 최초 활성화 차단선 설정
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
        // 0.1~0.2초 주기의 정밀 전술 틱 스타트
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
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    if (MyASC->HasMatchingGameplayTag(FTTags::FTStates::Debuff::Stunned))
    {
        if (AAIController* AIC = Cast<AAIController>(AvatarChar->GetController()))
        {
            AIC->StopMovement();
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
            
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // =========================================================================
            // 💡 [최적화 가드벽 - 추격 무빙 리패싱 맹점 정밀 완치]
            // 에셋 포커스 비교의 허점을 파쇄하고, 내비게이션 컴포넌트가 현재 추격 중인 
            // 실제 패스 최종 목적지와 타깃의 실시간 좌표를 피타고라스 오차 범위로 대조합니다.
            // =========================================================================
            bool bAlreadyMovingToTarget = false;
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                FVector TargetLoc = BestTarget->GetActorLocation();

                // 타깃이 이동 틱 오차 범위(예: 30cm) 내에 여전히 머물러 있다면 중복 Move 명령을 청정 스킵합니다.
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
        // 주위에 위협이 없다면 타깃 락 클리어 후 라인 복귀 연산 단행
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
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

    // 💡 [사망/종료 시 브레인 정화 완착]:
    // 미니언이 사망 판정을 받았거나 브레인이 닫힐 때, AI 컨트롤러가 잡고 있던 
    // 잔여 조준(Focus) 수식어를 강제 해제하여 시체 시선 고정 버그를 청정 소각합니다.
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