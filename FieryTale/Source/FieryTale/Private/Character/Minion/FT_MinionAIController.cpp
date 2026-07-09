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
    // 틱을 완전히 꺼버려서 매 프레임 발생하는 AI 연산 오버헤드를 0으로 박멸합니다.
    // 우리의 뇌세포(GAS) 타이머가 지정된 주기에만 깨어나 통제할 것입니다.
    PrimaryActorTick.bCanEverTick = false;

    // 1단계: 순정 AIPerception 인프라 조립 공정 타설 (상속 변수명 충돌 방지선 개통)
    MinionPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("MinionPerceptionComponent"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

    if (SightConfig && MinionPerceptionComponent)
    {
        // 2단계: 정면 전투 특화 시야각 및 색적 범위 기입 (FieryTale 미니언 표준 스펙)
        SightConfig->SightRadius = 1500.f;       // 미니언의 최대 색적 범위 (15미터)
        SightConfig->LoseSightRadius = 1800.f;   // 타깃을 놓치는 한계 범위
        SightConfig->PeripheralVisionAngleDegrees = 90.f; // 전방 180도 전체를 커버하는 광폭 시야각
        SightConfig->SetMaxAge(3.f);             // 감지된 기억의 유효 수명
        
        // 피아식별 태그를 C++ 내부에서 정밀 필터링하므로, 엔진 기본 소속 분류는 전량 개방합니다.
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

        // 조립된 시각 설정을 마스터 컴포넌트에 완착 동기화합니다.
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
        
        // 내장 블랙보드가 없다면 깨끗하게 새로 생성하여 장부를 개통합니다.
        if (!BlackboardComp)
        {
            UseBlackboard(MinionBlackboardAsset, BlackboardComp);
        }
        
        if (BlackboardComp)
        {
            // [초기화 가드] 스포너가 좌표를 찍어주기 전까지 라인 웨이포인트를 안전하게 제로 벡터로 리셋해 둡니다.
            BlackboardComp->SetValueAsVector(TEXT("LineWaypoint"), FVector::ZeroVector);
            
            // 타깃 오브젝트 키값도 안전하게 초기화합니다.
            BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), nullptr);
        }
    }

    // 주권 서버 권한 하에서만 인지 감지 센서 델리게이트 밸브를 개통합니다.
    if (HasAuthority() && MinionPerceptionComponent)
    {
        MinionPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AFT_MinionAIController::OnMinionPerceptionUpdated);
    }
}

void AFT_MinionAIController::OnMinionPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor || !Stimulus.IsValid()) return;

    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp) return;

    // 시야에서 적을 놓친 경우 (Successfully Sensed가 아닐 때)
    if (!Stimulus.WasSuccessfullySensed())
    {
        // 현재 쫓던 타깃을 놓친 게 맞다면, 블랙보드 타깃 장부를 비워서 브레인 GA가 판단할 수 있도록 동기화합니다.
        if (BlackboardComp->GetValueAsObject(TEXT("TargetEnemy")) == Actor)
        {
            BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), nullptr);
        }
        return;
    }

    // 이미 타깃을 락온하고 교전 중이라면 불필요한 필터링 연산 낭비를 막기 위해 패스합니다.
    if (BlackboardComp->GetValueAsObject(TEXT("TargetEnemy")) != nullptr) return;

    APawn* PossessedPawn = GetPawn();
    if (!PossessedPawn) return;

    // 3단계: FieryTale 진영 태그 기반 피아식별 검문소 가동
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

        // 같은 팀(아군 영웅, 아군 미니언)으로 판정되면 타깃팅에서 완벽히 배제합니다.
        if (bIsSameTeam)
        {
            return;
        }

        // 4단계: 적 진영 확정 장부 기입! 블랙보드 "TargetEnemy" 키에 밀봉 주입합니다.
        // 이제 브레인 GA(UFT_Minion_Brain)가 주기적으로 깨어나 이 블랙보드 장부를 인양해 무브와 공격을 통제합니다.
        BlackboardComp->SetValueAsObject(TEXT("TargetEnemy"), Actor);
    }
}