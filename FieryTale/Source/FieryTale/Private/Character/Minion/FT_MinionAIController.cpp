// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameplayTags/FTTags.h"

AFT_MinionAIController::AFT_MinionAIController()
{
    // 매 프레임 발생하는 AI 연산 부하를 차단하기 위해 틱 비활성화
    // 브레인 어빌리티(UFT_Minion_Brain) 타이머가 주기적으로 연산 통제
    PrimaryActorTick.bCanEverTick = false;

    // 1단계: 순정 AIPerception 인프라 및 시각 센서 컴포넌트 생성
    MinionPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("MinionPerceptionComponent"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

    if (SightConfig && MinionPerceptionComponent)
    {
        // 2단계: 미니언 표준 시각 및 색적 범위 설정
        SightConfig->SightRadius = 1500.f;                // 최대 색적 반경 (15미터)
        SightConfig->LoseSightRadius = 1800.f;            // 타깃 상실 한계 반경
        SightConfig->PeripheralVisionAngleDegrees = 90.f; // 전방 시야각 180도 커버
        SightConfig->SetMaxAge(3.f);                      // 인지된 자극의 기억 유효 수명
        
        // 피아식별은 C++ 내부 GAS 태그로 정밀 필터링하므로, 엔진 기본 소속 분류는 모두 개방
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

        // 조립된 시각 설정을 퍼셉션 컴포넌트에 동기화 및 주 감지선으로 등록
        MinionPerceptionComponent->ConfigureSense(*SightConfig);
        MinionPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    }
}

void AFT_MinionAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (MinionBlackboardAsset)
    {
        UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
        
        // 내장 블랙보드 컴포넌트가 없을 경우 새로 생성하여 에셋 바인딩
        if (!BlackboardComp)
        {
            UseBlackboard(MinionBlackboardAsset, BlackboardComp);
        }
        
        if (BlackboardComp)
        {
            // 초기화 가드: 경로 무빙 및 타깃 오브젝트 키값 안전 리셋
            BlackboardComp->SetValueAsVector(TEXT("LineWaypoint"), FVector::ZeroVector);
            BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), nullptr);
        }
    }

    // OnPossess는 서버 전용이므로 HasAuthority() 분기 없이 타깃 퍼셉션 업데이트 델리게이트 바인딩
    if (MinionPerceptionComponent)
    {
        MinionPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AFT_MinionAIController::OnMinionPerceptionUpdated);
    }
}

void AFT_MinionAIController::OnMinionPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor || !Stimulus.IsValid()) return;

    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp) return;

    // 시야에서 타깃을 놓친 경우 (Successfully Sensed 상태가 아닐 때)
    if (!Stimulus.WasSuccessfullySensed())
    {
        // 현재 추격 중이던 타깃과 일치하면 블랙보드 장부를 비워 브레인 연산이 인지하도록 동기화
        if (BlackboardComp->GetValueAsObject(TEXT("TargetEnemy")) == Actor)
        {
            BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), nullptr);
        }
        return;
    }

    // 이미 타깃을 락온하여 교전 중인 경우 불필요한 필터링 연산 방지를 위해 패스
    if (BlackboardComp->GetValueAsObject(TEXT("TargetEnemy")) != nullptr) return;

    APawn* PossessedPawn = GetPawn();
    if (!PossessedPawn) return;

    // 3단계: FieryTale 진영 태그 기반 피아식별 필터 검문
    UAbilitySystemComponent* MyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PossessedPawn);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

    if (MyASC && TargetASC)
    {
        bool bIsSameTeam = false;

        // 블루팀 진영 필터링
        if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && 
            TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
        {
            bIsSameTeam = true;
        }
        // 레드팀 진영 필터링
        else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && 
            TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
        {
            bIsSameTeam = true;
        }

        // 같은 진영(아군 영웅, 아군 미니언)으로 판정되면 타깃팅 대상에서 완전 배제
        if (bIsSameTeam)
        {
            return;
        }

        // 4단계: 적 진영 확정 시 블랙보드 "TargetEnemy" 키에 데이터 주입
        // 이후 브레인 GA(UFT_Minion_Brain)가 해당 장부를 인양하여 주기적으로 무브 및 공격 통제
        BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), Actor);
    }
}