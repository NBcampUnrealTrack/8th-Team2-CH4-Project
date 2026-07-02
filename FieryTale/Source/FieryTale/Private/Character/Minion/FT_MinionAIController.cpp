// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

AFT_MinionAIController::AFT_MinionAIController()
{
    // 틱을 완전히 꺼버려서 매 프레임 발생하는 AI 연산 오버헤드를 0으로 박멸합니다.
    // 우리의 뇌세포(GAS) 타이머가 지정된 주기에만 깨어나 통제할 것입니다.
    PrimaryActorTick.bCanEverTick = false;
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
        }
    }
}