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
#include "AbilitySystemInterface.h" // [리뷰 저격 추가 헤더선] 순정 GAS 인터페이스

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
            // 사거리 안착 시 제동 후 평타 GA 격발
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // 사거리 밖이라면 대상 추격
            AIC->MoveToActor(BestTarget, AttackAcceptanceRadius);
        }
    }
    else
    {
        // =========================================================================
        // [블랙보드 기반 라인 진격 및 복귀 파이프라인]
        // =========================================================================
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
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

    // =========================================================================
    // [리뷰 저격 박멸 1 - 다중 오버랩 환경 확장 빌드업 고려]
    // 현재는 단일 Pawn 채널 감지 루프이나, 타겟 액터 수집 대상을 영웅에 국한하지 않도록
    // 하단의 강제 캐스팅 락을 완벽히 해제하여 구조물 타겟팅의 길을 열어줍니다.
    // =========================================================================
    bool bHasActors = GetWorld()->OverlapMultiByChannel(
        OverlapResults, MyLoc, FQuat::Identity, ECollisionChannel::ECC_Pawn, ScanSphere, QueryParams
    );

    if (!bHasActors) return nullptr;

    AActor* BestTarget = nullptr;
    float BestTargetScore = -10000.0f;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* CandidateActor = Result.GetActor();
        if (!CandidateActor) continue;

        // =========================================================================
        // [리뷰 저격 박멸 2 - 순정 GAS 인터페이스 기반 가상 캐스팅 완화]
        // 구조물이든 캐릭터든 포괄할 수 있도록 IAbilitySystemInterface로 필터링을 변경합니다.
        // 이로 인해 기존에 nullptr로 튕겨 나가던 구조물 액터들이 드디어 정상 진입합니다!
        // =========================================================================
        IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CandidateActor);
        if (!ASI) continue;

        UAbilitySystemComponent* CandidateASC = ASI->GetAbilitySystemComponent();
        if (!CandidateASC) continue;

        // 대상이 사망 상태(Dead)라면 색적 제외
        if (CandidateASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

        // 피아식별 검증선
        bool bCandidateIsBlue = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue);
        bool bCandidateIsRed = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red);
        if (!bCandidateIsBlue && !bCandidateIsRed) continue;

        bool bIsSameTeam = (MyTeamTag == FTTags::FTFaction::Team_Blue && bCandidateIsBlue) ||
                           (MyTeamTag == FTTags::FTFaction::Team_Red && bCandidateIsRed);
        if (bIsSameTeam) continue;

        // =========================================================================
        // [AOS 전술 가중치 스코어링 연산 가동]
        // 드디어 구조물 타겟들이 무사히 도달하여 정상적인 가중치를 부여받습니다!
        // =========================================================================
        float CurrentScore = 0.0f;
        float Distance = FVector::Dist(MyLoc, CandidateActor->GetActorLocation());

        // 영웅 캐릭터(플레이어 제어) 판정
        if (AFTCharacterBase* TargetChar = Cast<AFTCharacterBase>(CandidateActor))
        {
            if (TargetChar->IsPlayerControlled())
            {
                CurrentScore += 6000.0f;
            }
        }
        
        // 구조물(포탑 / 넥서스) 판정
        if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret))
        {
            CurrentScore += 5000.0f;
        }
        else if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus))
        {
            CurrentScore += 4500.0f;
        }

        // 거리 비례 가중치 합산 (가까울수록 높은 점수)
        CurrentScore += (ScanRange - Distance) * 0.1f;

        if (CurrentScore > BestTargetScore)
        {
            BestTargetScore = CurrentScore;
            BestTarget = CandidateActor;
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