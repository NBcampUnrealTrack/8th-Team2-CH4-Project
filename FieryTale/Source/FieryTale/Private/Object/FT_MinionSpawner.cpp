// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_MinionSpawner.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AI/NavigationSystemBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"

AFT_MinionSpawner::AFT_MinionSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);
    bIsAlreadySpawning = false;
}

void AFT_MinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    
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

    TArray<UFT_MinionData*> NewWaveQueue;

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
        // 1단계: 런타임 실물 무빙 명령 대기 큐인 ActiveDataQueue에 데이터를 선제 안전 적재
        ActiveDataQueue.Append(NewWaveQueue);

        // 💡 [섀도잉 버그 완전 소각]: 앞의 'bool' 키워드를 완전히 제거하여
        // 클래스 장부의 진짜 멤버 변수인 bIsAlreadySpawning 상태를 무결하게 실시간 동기화합니다.
        bIsAlreadySpawning = GetWorld()->GetTimerManager().IsTimerActive(SequentialSpawnTimerHandle);

        // 2단계: 새로 유입된 웨이브 에셋들의 소프트 오브젝트 패스(비동기 주소) 취합
        TArray<FSoftObjectPath> AssetsToLoad;
        for (UFT_MinionData* Data : NewWaveQueue)
        {
            if (Data)
            {
                if (!Data->MinionMesh.IsNull())
                {
                    AssetsToLoad.AddUnique(Data->MinionMesh.ToSoftObjectPath());
                }
                if (!Data->MinionAnimClass.IsNull())
                {
                    AssetsToLoad.AddUnique(Data->MinionAnimClass.ToSoftObjectPath());
                }
            }
        }

        // 3단계: 소프트 패스가 존재한다면 비동기 스트리밍 로드 파이프라인 사출
        if (AssetsToLoad.Num() > 0)
        { 
            FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
            
            TSharedPtr<FStreamableHandle> NewHandle = StreamableManager.RequestAsyncLoad(
                AssetsToLoad, 
                FStreamableDelegate::CreateUObject(this, &AFT_MinionSpawner::OnMinionAssetsLoaded, AssetsToLoad, SpawnerTeamTag)
            );

            if (NewHandle.IsValid())
            {
                ActiveStreamableHandles.Add(NewHandle);
            }
        } 
        else
        {
            OnMinionAssetsLoaded(AssetsToLoad, SpawnerTeamTag);
        }
    }
}

void AFT_MinionSpawner::OnMinionAssetsLoaded(TArray<FSoftObjectPath> LoadedAssetPaths, FGameplayTag TeamTag)
{
    if (!GetWorld()) return;

    ActiveStreamableHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle) {
        return !Handle.IsValid() || Handle->IsActive() == false;
    });

    // 💡 마스터 상태 장부 재정산
    bIsAlreadySpawning = GetWorld()->GetTimerManager().IsTimerActive(SequentialSpawnTimerHandle);

    // 2단계: 메인 스레드 락 프리 개통 검문
    if (!bIsAlreadySpawning)
    {
        // 💡 사출 루프 시작과 동시에 클래스 상태를 확실하게 True로 밀봉 락인
        bIsAlreadySpawning = true;

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
        
        // 💡 모든 큐 사출이 완전히 마감된 시점에 정확하게 마스터 장부 밸브를 False로 리셋합니다.
        bIsAlreadySpawning = false; 
        return;
    }

    UFT_MinionData* TargetData = ActiveDataQueue[0];
    ActiveDataQueue.RemoveAt(0);

    if (!TargetData) return;

    // 물리 동결 방어선 타설: 스포너 위치 기준 좌우 분산 오프셋 주입
    FVector SpawnerLocation = GetActorLocation();
    FRotator SpawnRotation = GetActorRotation();
    
    float RandomOffset = FMath::FRandRange(-60.0f, 60.0f);
    FVector AggregatedOffset = GetActorRightVector() * RandomOffset;
    
    FVector FinalSpawnLocation = SpawnerLocation + AggregatedOffset;
    FTransform SpawnTransform(SpawnRotation, FinalSpawnLocation);
    
    AFT_MinionCharacterBase* SpawnedMinion = GetWorld()->SpawnActorDeferred<AFT_MinionCharacterBase>(
        MasterMinionClass, 
        SpawnTransform, 
        this,                                                           
        nullptr,                                                        
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn 
    );
    
    if (SpawnedMinion)
    {
        // 1단계: 마스터 진영 태그 및 조립용 데이터 에셋 하달
        SpawnedMinion->SetMinionDataAndTeam(TargetData, SpawnerTeamTag);
        
        // 2단계: AOS 전선 진격을 위한 최초 공성 거점(웨이포인트) 주소지 확정 낙인
        SpawnedMinion->SetCurrentTargetWayPoint(InitialWayPoint);
        
        // 3단계: [Stuck 및 Replication Pop 원천 박멸] FinishSpawning 이전에 정확한 바닥 위치 연산 집행
        FTransform FinalSpawnTransform = SpawnTransform;
        
        if (GetWorld() && SpawnedMinion->GetCapsuleComponent())
        {
            FVector StartLoc = SpawnTransform.GetLocation();
            FVector EndLoc = StartLoc - FVector(0.0f, 0.0f, 1000.0f);
            
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);
            QueryParams.AddIgnoredActor(SpawnedMinion);
            
            FHitResult FloorHit;
            if (GetWorld()->LineTraceSingleByChannel(FloorHit, StartLoc, EndLoc, ECC_Visibility, QueryParams))
            {
                float CapsuleHalfHeight = SpawnedMinion->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                FVector CorrectionLocation = FloorHit.ImpactPoint + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);
                
                if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
                {
                    FNavLocation ProjectedNavLoc;
                    if (NavSys->ProjectPointToNavigation(CorrectionLocation, ProjectedNavLoc, FVector(100.f, 100.f, 100.f)))
                    {
                        CorrectionLocation = ProjectedNavLoc.Location + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);
                    }
                }
                
                FinalSpawnTransform.SetLocation(CorrectionLocation);
            }
        }
        
        // 4단계: 진짜 바닥 좌표가 주입된 트랜스폼으로 월드 최종 완착 선언
        SpawnedMinion->FinishSpawning(FinalSpawnTransform);

        // 5단계: 바닥 완착 확인 후 무브먼트 모드 세팅 및 정산
        if (UCharacterMovementComponent* MoveComp = SpawnedMinion->GetCharacterMovement())
        {
            MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
            if (SpawnedMinion->GetCapsuleComponent())
            {
                MoveComp->FindFloor(SpawnedMinion->GetCapsuleComponent()->GetComponentLocation(), MoveComp->CurrentFloor, false);
            }
        }

        // 6단계: [AI 컨트롤러 주권 생성 및 강제 포제스 완착]
        if (HasAuthority())
        {
            SpawnedMinion->SpawnDefaultController();
        }
    }
}