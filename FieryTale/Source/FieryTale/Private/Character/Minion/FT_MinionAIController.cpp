// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionAIController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_WayPoint.h"

AFT_MinionAIController::AFT_MinionAIController()
{
    PrimaryActorTick.bCanEverTick = false;
    TargetEnemyActor = nullptr; // 순수 포인터 장부 초기화

    MinionPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("MinionPerceptionComponent"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

    if (SightConfig && MinionPerceptionComponent)
    {
        SightConfig->SightRadius = 1500.f;
        SightConfig->LoseSightRadius = 1800.f;
        SightConfig->PeripheralVisionAngleDegrees = 90.f;
        SightConfig->SetMaxAge(3.f);
        
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

        MinionPerceptionComponent->ConfigureSense(*SightConfig);
        MinionPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    }
}

void AFT_MinionAIController::BeginPlay()
{
    Super::BeginPlay();

    // 💡 [중복 바인딩 안티패턴 원천 박멸]:
    // OnPossess에 배치되어 폰 결속 시마다 중복 등록되던 델리게이트 스팸 리스크를 완전히 소각합니다.
    // 컨트롤러 생애 주기 중 단 1회만 호출되는 BeginPlay 단계에서 퍼셉션 인프라를 클린 바인딩합니다.
    if (MinionPerceptionComponent)
    {
        MinionPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AFT_MinionAIController::OnMinionPerceptionUpdated);
    }
}

void AFT_MinionAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // 💡 [블랙보드 소각 마감 및 배관 청소]:
    // 중복 호출 위험 요소를 BeginPlay로 격리 이관함에 따라 이 구역은 순정 폰 주권 인수 작업만 깔끔하게 집행합니다.
}

void AFT_MinionAIController::OnMinionPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    // 뇌사 락온 배관 전면 소각 처분 완료
    // 컨트롤러의 퍼셉션 컴포넌트는 오직 엔진 순정 상태로 현재 시야 내의 액터들을 
    // GetCurrentlyPerceivedActors() 장부에 실시간 적재/갱신하는 역할만 무결하게 수행합니다.
    // 진짜 타깃 선정 및 사망 검증(Dead Tag)은 브레인(UFT_Minion_Brain) 틱에서 실시간 스캔합니다.
}