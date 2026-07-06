// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_MinionSpawner.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemInterface.h" 

AFT_MinionSpawner::AFT_MinionSpawner()
    : WaveInterval(30.0f)            // 기획 명세: 다음 미니언 군단(웨이브) 격발 전까지의 대기 주기 (30초)
    , MeleeMinionCountPerWave(3)     // 기획 명세: 웨이브당 생산할 전방 돌격형 근접 미니언 개체 수
    , RangeMinionCountPerWave(3)     // 기획 명세: 웨이브당 생산할 후방 화력 지원형 원거리 미니언 개체 수
    , SpawnDelayBetweenMinions(0.5f) // 기획 명세: 미니언들이 스폰될 때 서로 겹쳐서 폭사하지 않도록 주는 정밀 사출 간격 (0.5초)
    , RemainingMeleeToSpawn(0)
    , RemainingRangeToSpawn(0)
{
    // 최적화 정책: 본체 스포너는 연산 부하를 줄이기 위해 상시 실시간 틱(Tick) 연산을 억제 차단합니다.
    PrimaryActorTick.bCanEverTick = false;
}

void AFT_MinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    if (GetWorld())
    {
        // 월드 진입과 동시에 게임 스타트 첫 번째 미니언 웨이브 시퀀스를 즉시 개통합니다.
        StartWaveSequence();

        // 기획서에 명시된 웨이브Interval(30초) 주기에 맞춰 상시 무한 루프로 돌아갈 마스터 타이머를 가동합니다.
        GetWorld()->GetTimerManager().SetTimer(
            WaveTimerHandle, this, &AFT_MinionSpawner::StartWaveSequence, WaveInterval, true
        );
    }
}

void AFT_MinionSpawner::StartWaveSequence()
{
    // 마스터 타이머가 띵 하고 울릴 때마다 이번 웨이브에 사출해야 할 잔여 장부 개수를 재충전합니다.
    RemainingMeleeToSpawn = MeleeMinionCountPerWave;
    RemainingRangeToSpawn = RangeMinionCountPerWave;

    if (GetWorld())
    {
        // 안전 조치: 혹시라도 이전 잔여 사출 서브 타이머가 돌고 있다면 런타임 꼬임 방지를 위해 청정 청소합니다.
        GetWorld()->GetTimerManager().ClearTimer(MinionSpawnSubTimerHandle);

        // 지정된 간격(0.5초)마다 SpawnSingleMinion 함수를 1개체씩 순차 격발할 서브 타이머를 루프로 개통합니다.
        GetWorld()->GetTimerManager().SetTimer(
            MinionSpawnSubTimerHandle, this, &AFT_MinionSpawner::SpawnSingleMinion, SpawnDelayBetweenMinions, true
        );
    }
}

void AFT_MinionSpawner::SpawnSingleMinion()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    TSubclassOf<AFT_MinionCharacterBase> ClassToSpawn = nullptr;

    // [미니언 사출 최우선 순위 분기 가동]
    // AOS 대칭 전장 철칙에 따라 전방 몸빵을 집도할 근접 군단을 먼저 전량 사출한 뒤 원거리를 순차 배정합니다.
    if (RemainingMeleeToSpawn > 0)
    {
        ClassToSpawn = MeleeMinionClass;
        RemainingMeleeToSpawn--;
    }
    else if (RemainingRangeToSpawn > 0)
    {
        ClassToSpawn = RangeMinionClass;
        RemainingRangeToSpawn--;
    }

    // 장부에 적힌 이번 웨이브 미니언 군단(근접3 + 원거리3 = 총 6마리) 사출이 완벽히 끝난 상태인지 검수
    if (!ClassToSpawn)
    {
        // 이번 웨이브 할당량을 모두 채웠으므로 다음 30초 대형 웨이브 타이머 전까지 서브 사출 타이머를 닫고 휴지기에 돌입합니다.
        World->GetTimerManager().ClearTimer(MinionSpawnSubTimerHandle);
        return;
    }

    // 스포너 액터가 배치된 현재 월드 좌표와 회전각을 사출 원점으로 동기화
    FVector SpawnLocation = GetActorLocation();
    FRotator SpawnRotation = GetActorRotation();
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    // 충돌 처리 옵션: 미니언들이 스폰 순간 살짝 부딪히더라도 스폰 억제당하지 않고 약간 조정해서든 무조건 월드에 태어나도록 강제 조정
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    // 마스터 미니언 베이스 C++ 규격으로 월드에 최종 실물 액터 사출
    AFT_MinionCharacterBase* SpawnedMinion = World->SpawnActor<AFT_MinionCharacterBase>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
    
    // [GAS 순정 소속 진영 각인 파이프라인 가동]
    // 미니언이 정상적으로 태어났고 스포너에 기획자가 낙인찍어둔 팀 태그(Blue 혹은 Red)가 유효하다면 피아식별 코드를 이식합니다.
    if (SpawnedMinion && SpawnerTeamTag.IsValid())
    {
        // 미니언 본체가 GAS 아키텍처(Interface)를 안전하게 준수하는 개체인지 장부 검수
        if (IAbilitySystemInterface* GASInterface = Cast<IAbilitySystemInterface>(SpawnedMinion))
        {
            if (UAbilitySystemComponent* MinionASC = GASInterface->GetAbilitySystemComponent())
            {
                // 미니언의 ASC 컴포넌트에 실시간 루즈 태그 형태로 소속 진영(예: FTTags::FTFaction::Team_Blue)을 영구 각인합니다.
                // 이 과정을 통해 영웅들의 레이더 스캔이나 미니언 상호간의 오폭 없는 정밀 피아식별이 비로소 완성됩니다.
                MinionASC->AddLooseGameplayTag(SpawnerTeamTag);
            }
        }
    }
}