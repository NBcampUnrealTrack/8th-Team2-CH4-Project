// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_MinionSpawner.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

AFT_MinionSpawner::AFT_MinionSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // 네트워킹 인프라 완착: 스포너 자체의 타이머 제어와 주권 통제를 세션 전역에 일치시킵니다.
    bReplicates = true;
}

void AFT_MinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    // [주권 검문소 가드선 타설]: 오직 데디케이트/호스트 서버 권한 하에서만 웨이브 정기 타이머를 개통합니다.
    if (HasAuthority() && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            WaveTimerHandle,
            this,
            &AFT_MinionSpawner::TriggerMinionWave,
            WaveInterval,
            true,
            1.0f
        );
    }
}

void AFT_MinionSpawner::TriggerMinionWave()
{
    if (!GetWorld() || !InitialWayPoint || !MasterMinionClass) return;

    // 새로운 웨이브 조립을 위해 가동 중이던 순차 스폰 파이프라인을 깨끗하게 포맷 청소합니다.
    GetWorld()->GetTimerManager().ClearTimer(SequentialSpawnTimerHandle);
    ActiveDataQueue.Empty();

    // 이번 웨이브에 진격할 미니언 데이터 에셋 장부를 큐에 순서대로 밀봉합니다.
    if (MeleeMinionData)
    {
        for (int32 i = 0; i < MeleeMinionCount; ++i)
        {
            ActiveDataQueue.Add(MeleeMinionData);
        }
    }

    if (RangedMinionData)
    {
        for (int32 i = 0; i < RangedMinionCount; ++i)
        {
            ActiveDataQueue.Add(RangedMinionData);
        }
    }

    if (ActiveDataQueue.Num() > 0)
    {
        // =========================================================================
        // [비동기 에셋 프리로드 파이프라인 가동]
        // 순차 사출 도중 발생하는 프레임 드롭을 박멸하기 위해, 이번 웨이브에 사용될 
        // 미니언들의 소프트 메쉬 주소들을 수집하여 비동기 스트리밍 로드를 선제 단행합니다.
        // =========================================================================
        TArray<FSoftObjectPath> AssetsToLoad;
        for (UFT_MinionData* Data : ActiveDataQueue)
        {
            if (Data && !Data->MinionMesh.IsNull())
            {
                AssetsToLoad.AddUnique(Data->MinionMesh.ToSoftObjectPath());
            }
        }

        if (AssetsToLoad.Num() > 0)
        {
            // 엔진 순정 에셋 매니저의 스트리밍 파이프라인을 호출합니다.
            FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
            
            // 비동기 로드가 완공(Load 완료)되는 순간 안전하게 순차 사출 함수가 깨어나도록 바인딩합니다.
            StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateUObject(this, &AFT_MinionSpawner::StartSequentialSpawn));
        }
        else
        {
            // 로드할 비주얼 자산이 없다면 즉시 사출 회로를 개통합니다.
            StartSequentialSpawn();
        }
    }
}

void AFT_MinionSpawner::StartSequentialSpawn()
{
    if (!GetWorld()) return;

    // 비동기 인양이 무결하게 마감되었으므로, 기획된 지정 딜레이 간격에 맞춰 정밀 순차 사출 타이머를 격발합니다.
    GetWorld()->GetTimerManager().SetTimer(
        SequentialSpawnTimerHandle,
        this,
        &AFT_MinionSpawner::SpawnMinionFromQueue,
        SpawnDelayBetweenMinions,
        true,
        0.0f
    );
}

void AFT_MinionSpawner::SpawnMinionFromQueue()
{
    if (!GetWorld() || ActiveDataQueue.Num() == 0)
    {
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SequentialSpawnTimerHandle);
        return;
    }

    UFT_MinionData* TargetData = ActiveDataQueue[0];
    ActiveDataQueue.RemoveAt(0);

    if (!TargetData) return;

    FVector SpawnLocation = GetActorLocation();
    FRotator SpawnRotation = GetActorRotation();
    FTransform SpawnTransform(SpawnRotation, SpawnLocation);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = nullptr;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 지연 생성 아키텍처 구동: 뼈대 액터만 월드에 배치된 진공 상태를 유도합니다.
    AFT_MinionCharacterBase* SpawnedMinion = GetWorld()->SpawnActorDeferred<AFT_MinionCharacterBase>(
        MasterMinionClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
    );
    
    if (SpawnedMinion)
    {
        // 1단계: 마스터 팀 진영 태그 및 조립용 데이터 에셋 하달
        SpawnedMinion->SetMinionDataAndTeam(TargetData, SpawnerTeamTag);
        
        // 2단계: AOS 전선 진격을 위한 최초 공성 거점(웨이포인트) 주소지 확정 낙인
        SpawnedMinion->SetCurrentTargetWayPoint(InitialWayPoint);
        
        // 3단계: 월드 완착 선언 및 GAS 인프라/AI 사고 회로 수동 정밀 가동 명령 사출
        SpawnedMinion->FinishSpawning(SpawnTransform);
        SpawnedMinion->LaunchMinionInfrastructure();
    }
}