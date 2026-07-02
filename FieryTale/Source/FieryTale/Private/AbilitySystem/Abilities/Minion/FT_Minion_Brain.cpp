// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_Minion_Brain.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFT_Minion_Brain::UFT_Minion_Brain()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 마스터 CC 가드선 완착]
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

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    if (!AIC) return;

    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    if (!MyASC || MyASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    FGameplayTag MyTeamTag;
    if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) MyTeamTag = FTTags::FTFaction::Team_Blue;
    else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) MyTeamTag = FTTags::FTFaction::Team_Red;

    if (!MyTeamTag.IsValid()) return;

    AActor* BestTarget = InternalScanBestTarget(AvatarChar, MyTeamTag);

    if (BestTarget)
    {
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            // [용접 완료 구역] 사거리 안착 시 제동 후 평타 GA 격발!
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // 사거리 밖이라면 끝까지 쫓아갑니다.
            AIC->MoveToActor(BestTarget, AttackAcceptanceRadius);
        }
    }
    else
    {
        // =========================================================================
        // [총괄님 지령 반영 - 블랙보드 기반 라인 진격 및 복귀 파이프라인]
        // =========================================================================
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
            
            // 블랙보드 벡터가 유효하게 채워져 있을 때만 라인 무빙 하달
            if (!NextLineLocation.IsZero())
            {
                AIC->MoveToLocation(NextLineLocation, 10.0f);
            }
        }
    }
}

AActor* UFT_Minion_Brain::InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag)
{
    FVector MyLoc = AvatarChar->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(ScanRange);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(AvatarChar);

    bool bHasActors = GetWorld()->OverlapMultiByChannel(
        OverlapResults, MyLoc, FQuat::Identity, ECollisionChannel::ECC_Pawn, ScanSphere, QueryParams
    );

    if (!bHasActors) return nullptr;

    AActor* BestTarget = nullptr;
    float BestTargetScore = -10000.0f;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AFTCharacterBase* Candidate = Cast<AFTCharacterBase>(Result.GetActor());
        if (!Candidate) continue;

        UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent();
        if (!CandidateASC) continue;

        //  [네임스페이스 가드선 복구] FTTags:: 스코프 추가 완착
        if (CandidateASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

        bool bCandidateIsBlue = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue);
        bool bCandidateIsRed = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red);
        if (!bCandidateIsBlue && !bCandidateIsRed) continue;

        bool bIsSameTeam = (MyTeamTag == FTTags::FTFaction::Team_Blue && bCandidateIsBlue) ||
                           (MyTeamTag == FTTags::FTFaction::Team_Red && bCandidateIsRed);
        if (bIsSameTeam) continue;

        // =========================================================================
        // [AOS 전술 가중치 연산 주회선]
        // =========================================================================
        float CurrentScore = 0.0f;
        float Distance = FVector::Dist(MyLoc, Candidate->GetActorLocation());

        if (Candidate->IsPlayerControlled()) CurrentScore += 6000.0f;
        else if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret)) CurrentScore += 5000.0f;
        else if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus)) CurrentScore += 4500.0f;

        CurrentScore += (ScanRange - Distance) * 0.1f;

        if (CurrentScore > BestTargetScore)
        {
            BestTargetScore = CurrentScore;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

void UFT_Minion_Brain::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}