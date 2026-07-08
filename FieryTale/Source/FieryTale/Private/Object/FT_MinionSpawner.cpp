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
    
    // 언리얼 5 순정 리플리케이션 가동 표준 명세로 락인
    SetReplicates(true);
}

void AFT_MinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    // [주권 검문소 가드선]: 오직 권한 하에 있는 주권 서버(Server)만 웨이브 타이머를 개통 구동합니다.
    if (HasAuthority() && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            WaveTimerHandle,
            this,
            &AFT_MinionSpawner::TriggerMinionWave,
            WaveInterval,
            true,
            1.0f // 초도 딜레이 1초 안전망 안착
        );
    }
}

void AFT_MinionSpawner::TriggerMinionWave()
{
    if (!GetWorld() || !InitialWayPoint || !MasterMinionClass) return;

    // [레이스 컨디션 가드벽]: 이전 웨이브 사출 파이프라인이 아직 가동 중(큐가 남음)이라면,
    // 데이터를 강제 덮어쓰기 소각하지 않고 안전하게 큐의 뒤에 가산 스택으로 누적하여 누수를 방지합니다.
    bool bIsAlreadySpawning = GetWorld()->GetTimerManager().IsTimerActive(SequentialSpawnTimerHandle);

    TArray<UFT_MinionData*> NewWaveQueue;

    // 이번 웨이브에 투입될 전술 자산 장부 패킹
    if (MeleeMinionData)
    {
        for (int32 i = 0; i < MeleeMinionCount; ++i)
        {
            NewWaveQueue.Add(MeleeMinionData);
        }
    }

    if (RangedMinionData)
    {
        for (int32 i = 0; i < RangedMinionCount; ++i)
        {
            NewWaveQueue.Add(RangedMinionData);
        }
    }

    if (NewWaveQueue.Num() > 0)
    {
        // 마스터 전역 큐에 원자적으로 패킷 병합 이관
        ActiveDataQueue.Append(NewWaveQueue);

        // [비동기 에셋 프리로드 파이프라인] 동일 자산 중복 로드 연산 제로 최적화
        TArray<FSoftObjectPath> AssetsToLoad;
        for (UFT_MinionData* Data : NewWaveQueue)
        {
            if (Data && !Data->MinionMesh.IsNull())
            {
                AssetsToLoad.AddUnique(Data->MinionMesh.ToSoftObjectPath());
            }
        }

        if (AssetsToLoad.Num() > 0)
        {
            FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
            
            // 사출 회로가 이미 돌아가고 있다면 로드만 백그라운드로 돌리고 타이머 재점화는 기각합니다.
            if (!bIsAlreadySpawning)
            {
                StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateUObject(this, &AFT_MinionSpawner::StartSequentialSpawn));
            }
        }
        else
        {
            if (!bIsAlreadySpawning)
            {
                StartSequentialSpawn();
            }
        }
    }
}

void AFT_MinionSpawner::StartSequentialSpawn()
{
    if (!GetWorld()) return;

    // 안전망 검문: 중복 타이머 점화 필터링
    if (GetWorld()->GetTimerManager().IsTimerActive(SequentialSpawnTimerHandle)) return;

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
    
    AFT_MinionCharacterBase* SpawnedMinion = GetWorld()->SpawnActorDeferred<AFT_MinionCharacterBase>(
        MasterMinionClass, 
        SpawnTransform, 
        this,                                                           // Owner (스포너 본체 주소)
        nullptr,                                                        // Instigator
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn // 충돌 처리 정책 완착
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