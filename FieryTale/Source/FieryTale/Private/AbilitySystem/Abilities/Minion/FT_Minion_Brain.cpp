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

    // =========================================================================
    // 💡 [실시간 하드 CC기 가드선 배관 개통]
    // 어빌리티가 켜진 상태에서 런타임 도중 기절(Stunned) 등의 제어 불가 태그가 
    // 내 ASC 장부에 안착했다면, 이번 틱 사고 연산을 전면 소멸 거부합니다.
    // 이 방어선 덕분에 기절 중에 미니언이 평타를 연사하는 버그가 완전 멸균됩니다.
    // =========================================================================
    if (MyASC->HasMatchingGameplayTag(FTTags::FTStates::Debuff::Stunned))
    {
        // 상태 이상 중일 때는 이동 관성을 즉시 파쇄 제동합니다.
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

    // 현재 길찾기 이동 컴포넌트의 가동 상태를 역추적합니다.
    UPathFollowingComponent* PathFollowComp = AIC->GetPathFollowingComponent();

    AActor* BestTarget = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetEnemy")));

    if (BestTarget)
    {
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            // [교전 상태 전환]: 사거리 진입 시 물리 무빙 급제동 후 평타 사출
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // =========================================================================
            // [최적화 가드벽 - 추격 무빙 리패싱 제한 최종 완치]
            // =========================================================================
            bool bAlreadyMovingToTarget = false;
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                // 언리얼 순정 정석 패스: AI가 현재 포커싱하여 추격 중인 대상 액터가 
                // 블랙보드의 BestTarget과 일치한다면 중복 길찾기 명령을 스킵합니다.
                if (AIC->GetFocusActor() == BestTarget)
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

        // =========================================================================
        // 💡 [최적화 가드벽 - 라인 복귀 리패싱 제한 완치 완착]
        // =========================================================================
        FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
        if (!NextLineLocation.IsZero())
        {
            bool bAlreadyMovingToLoc = false;
            if (PathFollowComp && PathFollowComp->GetStatus() == EPathFollowingStatus::Moving)
            {
                // 언리얼 순정 API: PathFollowingComponent 내부에서 현재 기동 중인 최종 목적지 좌표 벡터를 안전하게 인양합니다.
                FVector CurrentDest = PathFollowComp->GetPathDestination();
                
                // FVector 고유의 오차 범위 비교 함수인 Equals를 활용하여 정밀 대조합니다.
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
    // 수명 마감 시 월드 정기 타이머 핸들을 깨끗하게 수거 청소합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}