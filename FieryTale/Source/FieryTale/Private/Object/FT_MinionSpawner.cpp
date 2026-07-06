// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_MinionSpawner.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFT_MinionSpawner::AFT_MinionSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AFT_MinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    if (GetWorld())
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

    GetWorld()->GetTimerManager().ClearTimer(SequentialSpawnTimerHandle);
    ActiveDataQueue.Empty();

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
        GetWorld()->GetTimerManager().SetTimer(
            SequentialSpawnTimerHandle,
            this,
            &AFT_MinionSpawner::SpawnMinionFromQueue,
            SpawnDelayBetweenMinions,
            true,
            0.0f
        );
    }
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
    
    // 이 스포너 액터는 APawn 상속 개체가 아니므로 잘못된 Cast를 제거하고 nullptr로 안전 마감합니다.
    SpawnParams.Instigator = nullptr;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 네 번째 인자값 자리에 오폭을 유도하던 무의미한 APawn 캐스팅 사족을 완전히 도려내고 nullptr로 치유했습니다.
    AFT_MinionCharacterBase* SpawnedMinion = GetWorld()->SpawnActorDeferred<AFT_MinionCharacterBase>(
        MasterMinionClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
    );
    
    if (SpawnedMinion)
    {
        SpawnedMinion->SetMinionDataAndTeam(TargetData, SpawnerTeamTag);
        
        SpawnedMinion->SetCurrentTargetWayPoint(InitialWayPoint);
        
        SpawnedMinion->FinishSpawning(SpawnTransform);
        SpawnedMinion->LaunchMinionInfrastructure();
    }
}