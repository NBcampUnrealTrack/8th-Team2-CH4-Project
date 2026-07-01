#include "Level/FTTurret.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Core/Game/FTGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"

AFTTurret::AFTTurret()
{
    PrimaryActorTick.bCanEverTick = true;                                   // 진동 및 하강 연출을 위해 틱 활성화

    TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh")); // 메시 서브오브젝트 생성
    RootComponent = TurretMesh;                                             // 메시를 루트 컴포넌트로 지정

    AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent")); // ASC 서브오브젝트 생성
    AbilitySystemComponent->SetIsReplicated(true);             // 멀티플레이 환경을 위해 ASC 복제 활성화
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet")); // 어트리뷰트 셋 서브오브젝트 생성

    TurretTeam = EFTTurretTeam::BlueTeam;                                   // 기본 진영을 블루팀으로 초기화
    TurretPosition = EFTTurretPosition::None;
    bIsDying = false;                                                       // 파괴 상태 플래그 초기화
    DyingTimer = 0.0f;                                                      // 파괴 타이머 초기화
    MaxDyingTime = 2.0f;                                                    // 파괴 연출에 소요될 시간을 2초로 설정
}

UAbilitySystemComponent* AFTTurret::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;                                       // GAS 인터페이스 통신을 위한 ASC 포인터 반환
}

void AFTTurret::BeginPlay()
{
    Super::BeginPlay();                                                     // 부모 클래스의 BeginPlay 호출

    if (AbilitySystemComponent)                                             // ASC 유효성 검증
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this); // 포탑 자신을 Owner와 Avatar로 지정하여 GAS 초기화

        FGameplayTag TeamTag = (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 진영에 따른 태그 산출
        AbilitySystemComponent->AddLooseGameplayTag(TeamTag);               // GEEC_Damage 피아식별용 태그 부착
        AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTObjectType::Structure_Turret);

        float TargetMaxHealth = 1000.0f;
        if (TurretDataTable && !DataRowName.IsNone())
        {
            FFTStructureData* RowData = TurretDataTable->FindRow<FFTStructureData>(DataRowName, TEXT(""));
            if (RowData)
            {
                TargetMaxHealth = RowData->MaxHealth;
            }
        }

        AttributeSet->InitMaxHealth(TargetMaxHealth);
        AttributeSet->InitHealth(TargetMaxHealth);

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTurret::OnHealthChanged); // 체력 증감 이벤트 바인딩
    }

    // BeginPlay 호출 후 10.0초 뒤에 DebugDestroyTurret 함수를 단 1회 실행하도록 타이머 설정
    GetWorldTimerManager().SetTimer(AutoDestroyTimerHandle, this, &AFTTurret::DebugDestroyTurret, 10.0f, false);
}

void AFTTurret::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);                                             // 부모 클래스의 틱 기능 유지

    if (bIsDying)                                                       // 파괴 연출 모드 진입 확인
    {
        DyingTimer += DeltaTime;                                        // 경과 시간 누적
        if (DyingTimer < MaxDyingTime)                                  // 연출 시간이 끝나지 않았다면
        {
            float DropProgress = DyingTimer / MaxDyingTime;             // 하강 진행도 0.0 ~ 1.0 산출
            FVector NewLocation = InitialLocation;                      // 기준 위치 복사
            NewLocation.Z -= DropProgress * 500.0f;                     // 시간에 비례하여 500 유닛 하강
            NewLocation.X += FMath::RandRange(-5.0f, 5.0f); // X축 랜덤 진동 적용
            NewLocation.Y += FMath::RandRange(-5.0f, 5.0f); // Y축 랜덤 진동 적용
            SetActorLocation(NewLocation);                              // 계산된 새로운 위치를 포탑에 반영
        }
        else                                                            // 연출 시간이 끝났다면
        {
            bIsDying = false;                                           // 파괴 연출 상태 해제
            SetActorHiddenInGame(true);                                 // 렌더링 끄기
            SetActorEnableCollision(false);                             // 모든 콜리전 충돌 판정 끄기
            SetActorTickEnabled(false);                                 // 틱 비활성화로 자원 확보
        }
    }
}

void AFTTurret::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Data.NewValue <= 0.0f && !bIsDying)                             // 체력이 0 이하이고 아직 파괴 연출 전이라면
    {
        DebugDestroyTurret();                                           // 파괴 함수 호출
    }
}

void AFTTurret::DebugDestroyTurret()
{
    if (bIsDying) return;                                               // 이미 파괴 중이라면 중복 실행 방지

    bIsDying = true;                                                    // 파괴 연출 모드 활성화
    InitialLocation = GetActorLocation();                               // 하강 시작 기준점을 현재 위치로 캐싱

    GetWorldTimerManager().ClearTimer(AutoDestroyTimerHandle);      // 연출 시작 시 혹시 남아있을 수 있는 타이머 스케줄을 명시적으로 해제

    if (DestructionSound)                                               // 사운드 에셋 유효성 검증
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 파괴 사운드 1회 재생
    }

    if (DestructionEffect)                                              // 파티클 에셋 유효성 검증
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 파괴 시각 이펙트 스폰
    }

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 포격 AI 정지를 위한 무력화 태그 부착
    }

    if (UWorld* World = GetWorld())
    {
        if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
        {
            GameMode->TurretDestroyed(this);
        }
    }
}