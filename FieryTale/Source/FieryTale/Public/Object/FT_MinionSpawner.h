// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "FT_MinionSpawner.generated.h"

class AFT_MinionCharacterBase;
class AFT_WayPoint;
class UFT_MinionData;

/**
 * 총괄님의 기획 명세: 단 1개의 마스터 미니언 캐릭터 클래스만 들고,
 * 데이터 에셋(MinionData)만 교체하여 탑/미드/바텀 레일로 무한 사출하는 순정 웨이브 스포너입니다.
 */
UCLASS()
class FIERYTALE_API AFT_MinionSpawner : public AActor
{
    GENERATED_BODY()
    
public:    
    AFT_MinionSpawner();

protected:
    virtual void BeginPlay() override;

    // =========================================================================
    // [AOS 웨이브 마스터 구성 인프라]
    // =========================================================================
    
    /** 스포너 자체의 소속 진영 태그 (예: FTTags::FTFaction::Team_Blue / Team_Red) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | Config")
    FGameplayTag SpawnerTeamTag;

    /** 미니언 대열이 태어나서 밟아야 할 첫 번째 초도 웨이포인트 주소지 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | Navigation")
    TObjectPtr<AFT_WayPoint> InitialWayPoint;

    /** 웨이브가 격발되는 정기 주기 (초 단위, 기본 30초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | Time")
    float WaveInterval = 30.0f;

    /** 대열 내에서 미니언이 한 마리씩 순차 소환되는 마진 간격 (초 단위, 기본 0.5초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | Time")
    float SpawnDelayBetweenMinions = 0.5f;

    // =========================================================================
    //  [총괄님 마스터 피스 슬롯]: 블루프린트는 딱 이 1개만 매핑합니다.
    // =========================================================================
    /** 무한 복제 생산의 소체가 될 청정한 마스터 미니언 블루프린트 클래스 (BP_MinionCharacter) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | CoreClass")
    TSubclassOf<AFT_MinionCharacterBase> MasterMinionClass;

    // =========================================================================
    // [웨이브 생산 수치 및 데이터 에셋 바구니]
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | WaveCount")
    int32 MeleeMinionCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | WaveCount")
    int32 RangedMinionCount = 3;

    /** 에디터에서 기획팀이 던져줄 근접 미니언 속성 자산 데이터 (DA_Minion_Melee) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | WaveData")
    TObjectPtr<UFT_MinionData> MeleeMinionData;

    /** 에디터에서 기획팀이 던져줄 원거리 미니언 속성 자산 데이터 (DA_Minion_Ranged) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Spawner | WaveData")
    TObjectPtr<UFT_MinionData> RangedMinionData;

private:
    /** 30초 주기로 웨이브 생산 종을 울리는 통제 함수 */
    void TriggerMinionWave();

    /** 대기 큐에서 데이터 에셋을 하나씩 꺼내 실물 육체에 주입 사출하는 엔진 함수 */
    void SpawnMinionFromQueue();

    /** 런타임에 소환 명령을 대기할 데이터 에셋 주소 보관 장부 큐 */
    UPROPERTY()
    TArray<TObjectPtr<UFT_MinionData>> ActiveDataQueue;

    FTimerHandle WaveTimerHandle;
    FTimerHandle SequentialSpawnTimerHandle;
};