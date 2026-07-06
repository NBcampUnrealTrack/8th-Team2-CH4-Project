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
#include "AbilitySystemInterface.h" 
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"

UFT_Minion_Brain::UFT_Minion_Brain()
{
    // 인스턴싱 정책: 미니언 개체별로 뇌세포 틱(타이머)을 독립 구동하기 위해 액터당 인스턴스 격리 생성
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 마스터 CC 가드선 완착]
    // 기절(Stunned) 디버프 상태 레이어가 가동 중일 때는 AI 사고 회로 자체를 셧다운 봉쇄합니다.
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
        // 기획 명세: 지정된 스캔인터벌 주기에 맞춰 미니언의 핵심 AI 전술 루프를 상시 무한 반복 구동합니다.
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
        // 미니언 사망 시 즉시 AI 작동 타이머를 소각 해제하고 뇌세포 어빌리티 종결
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    FGameplayTag MyTeamTag;
    if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) MyTeamTag = FTTags::FTFaction::Team_Blue;
    else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) MyTeamTag = FTTags::FTFaction::Team_Red;

    if (!MyTeamTag.IsValid()) return;

    // [1단계: 브레인 소속의 마스터 타깃 셀렉터 스캔 발동]
    AActor* BestTarget = InternalScanBestTarget(AvatarChar, MyTeamTag);

    if (BestTarget)
    {
        // 시야 내에 타깃 포착 시 무빙 포커스를 적으로 고정 (정면 상시 락온 상태 유도)
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            // [교전 상태]: 사거리 내부 도달 시 정지 후 평타 GA 격발 유도
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // [적 추격 상태]: 사거리 밖의 타깃을 끝까지 추격
            AIC->MoveToActor(BestTarget, AttackAcceptanceRadius);
        }
    }
    else
    {
        // =========================================================================
        // [2단계: 평화 진격 상태 - 캐릭터의 거점 장부를 파싱하여 블랙보드 갱신 동기화]
        // 주위에 적이 없다면 타깃 시선 락을 클리어하고, 캐릭터 육체가 보관 중인 레일 데이터를 
        // 브레인이 제어 분석하여 다음 웨이포인트 벡터 좌표로 스위칭 전진시킵니다.
        // =========================================================================
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            if (MinionChar)
            {
                // 미니언 본체 헤더에 증설해 둔 현재 타겟 웨이포인트 주소지 견인
                AFT_WayPoint* TargetWP = MinionChar->GetCurrentTargetWayPoint();
                if (TargetWP)
                {
                    float DistanceToWP = FVector::Dist(MinionChar->GetActorLocation(), TargetWP->GetActorLocation());
                    
                    // 거점 도달 반경 검수 및 다음 연결 고리 갱신 명령 피드백
                    if (DistanceToWP <= TargetWP->ArrivalRadius)
                    {
                        AFT_WayPoint* NextWP = TargetWP->NextWayPoint;
                        MinionChar->SetCurrentTargetWayPoint(NextWP);
                        TargetWP = NextWP;
                    }
                    
                    // 최종 유효 거점의 3D 공간 좌표 벡터 패킷을 순정 블랙보드 우체통에 동동 주입 주사합니다.
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

            // 정비된 블랙보드 목적지 벡터를 인양하여 라인 순찰 복귀 기동 지령 사출
            FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
            if (!NextLineLocation.IsZero())
            {
                AIC->MoveToLocation(NextLineLocation, 50.0f, true, true, true);
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

    // [리뷰 저격 박멸 1 - 확장 빌드업 개통]
    // 영웅 액터에만 필터링이 국한되지 않도록 단일 폰 채널 다이내믹 포괄 스캔
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

        // [리뷰 저격 박멸 2 - 순정 GAS 인터페이스 기반 가상 캐스팅 완화]
        // 타겟팅 대상을 캐릭터 군단에 락온하지 않고 포탑, 억제기, 넥서스 구조물까지 무결 포괄 인양합니다.
        IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CandidateActor);
        if (!ASI) continue;

        UAbilitySystemComponent* CandidateASC = ASI->GetAbilitySystemComponent();
        if (!CandidateASC) continue;

        // [오타 수선 마감 완료]: HasMatchingMatching... 중복 자구 오타를 순정 멤버명으로 청정 보수 완착
        if (CandidateASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

        // 동적 피아식별 필터링
        bool bCandidateIsBlue = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue);
        bool bCandidateIsRed = CandidateASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red);
        if (!bCandidateIsBlue && !bCandidateIsRed) continue;

        bool bIsSameTeam = (MyTeamTag == FTTags::FTFaction::Team_Blue && bCandidateIsBlue) ||
                           (MyTeamTag == FTTags::FTFaction::Team_Red && bCandidateIsRed);
        if (bIsSameTeam) continue;

        // =========================================================================
        // [AOS 전술 가중치 스코어링 연산 가동]
        // 포탑 및 주요 구조물 오브젝트들이 이 장부선에 정상 도달하여 가중치 배율을 배정받습니다.
        // =========================================================================
        float CurrentScore = 0.0f;
        float Distance = FVector::Dist(MyLoc, CandidateActor->GetActorLocation());

        // 영웅 캐릭터 가중치 (플레이어 제어 대상 최우선 점사 처단 가중치)
        if (AFTCharacterBase* TargetChar = Cast<AFTCharacterBase>(CandidateActor))
        {
            if (TargetChar->IsPlayerControlled())
            {
                CurrentScore += 6000.0f;
            }
        }
        
        // 공성 연산 구조물(포탑 / 넥서스) 식별 및 전술 가중치 합산
        if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Turret))
        {
            CurrentScore += 5000.0f;
        }
        else if (CandidateASC->HasMatchingGameplayTag(FTTags::FTObjectType::Structure_Nexus))
        {
            CurrentScore += 4500.0f;
        }

        // 거리 기반 비례 보정 합산 (가장 가까운 대상일수록 우선순위 가산)
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
    // 어빌리티 해제 소멸 시점에 안전하게 주기적 정기 AI 스캔 타이머 컴포넌트를 청정 소각 제거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}