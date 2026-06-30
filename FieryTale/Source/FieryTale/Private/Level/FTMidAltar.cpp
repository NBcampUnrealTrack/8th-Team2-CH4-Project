#include "Level/FTMidAltar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Materials/MaterialInstanceDynamic.h"

AFTMidAltar::AFTMidAltar()
{
    PrimaryActorTick.bCanEverTick = true; // 시뮬레이션 컬러 보간을 위해 틱 활성화

    AltarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AltarMesh")); // 메시 컴포넌트 생성
    RootComponent = AltarMesh; // 루트 컴포넌트 지정

    CaptureVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CaptureVolume")); // 콜리전 컴포넌트 생성
    CaptureVolume->SetupAttachment(RootComponent); // 루트에 부착
    CaptureVolume->SetCollisionProfileName(TEXT("Trigger")); // 콜리전 프로파일 트리거로 세팅

    AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent")); // ASC 생성
    AbilitySystemComponent->SetIsReplicated(true); // 복제 활성화
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // AI/오브젝트 규격 설정

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet")); // 어트리뷰트 생성

    AltarTeam = EFTAltarTeam::None; // 기본 진영 상태를 미점령(None)으로 초기화
    TeamColorParameterName = TEXT("TeamColor"); // 파라미터 연동 네임 기본값 설정
    
    NeutralColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f); // 기본 상태 색상을 하얀색으로 지정
    BlueTeamColor = FLinearColor(0.0f, 0.2f, 1.0f, 1.0f); // 블루팀 컬러 기본값 설정
    RedTeamColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f); // 레드팀 컬러 기본값 설정
    
    bEnableSimulation = true; // 에디터 가시성 테스트를 위해 초기값을 트루로 설정
    SimElapsedTime = 0.0f; // 시뮬레이션 타이머 초기화
}

UAbilitySystemComponent* AFTMidAltar::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent; // GAS 인터페이스 규격에 따른 ASC 반환
}

void AFTMidAltar::SetGenericTeamId(const FGenericTeamId& InTeamID)
{
    TeamId = InTeamID; // 팀 ID 설정
}

FGenericTeamId AFTMidAltar::GetGenericTeamId() const
{
    return TeamId; // 팀 ID 반환
}

void AFTMidAltar::BeginPlay()
{
    Super::BeginPlay(); // 부모 BeginPlay 호출

    CaptureVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTMidAltar::OnOverlapBegin); // 충돌 이벤트 바인딩

    if (AltarMesh && AltarMesh->GetMaterial(0)) // 머티리얼 유효성 확인
    {
        DynamicAltarMaterial = UMaterialInstanceDynamic::Create(AltarMesh->GetMaterial(0), this); // 다이내믹 인스턴스 생성
        if (DynamicAltarMaterial) // 인스턴스 생성 성공 검증
        {
            AltarMesh->SetMaterial(0, DynamicAltarMaterial); // 메시에 할당
            UpdateAltarColor(NeutralColor); // 초기 색상을 하얀색으로 설정
        }
    }

    if (AbilitySystemComponent) // GAS 초기화 검증
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this); // 액터 정보 설정
        AttributeSet->InitMaxHealth(10000.0f); // 최대 내구도 초기화
        AttributeSet->InitHealth(10000.0f); // 현재 내구도 초기화
    }
}

void AFTMidAltar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime); // 부모 틱 호출

    if (bEnableSimulation) // 시뮬레이션 플래그가 켜져 있을 때만 작동
    {
        HandleSimulation(DeltaTime); // 타임라인 동적 컬러 변환 실행
    }
}

void AFTMidAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bEnableSimulation && OtherActor && OtherActor != this) // 외부 액터 유효성 검증
    {
        bEnableSimulation = false; // 상호작용 발생 시 자동 시뮬레이션 즉각 정지
        UpdateAltarColor(NeutralColor); // 상태 조정을 위해 하얀색으로 초기화
    }
}

void AFTMidAltar::UpdateAltarColor(FLinearColor NewColor)
{
    if (DynamicAltarMaterial) // 다이내믹 머티리얼 유효성 검사
    {
        DynamicAltarMaterial->SetVectorParameterValue(TeamColorParameterName, NewColor); // 파라미터에 색상 적용
    }
}

void AFTMidAltar::HandleSimulation(float DeltaTime)
{
    SimElapsedTime += DeltaTime; // 경과 시간 누적

    if (SimElapsedTime <= 10.0f) // 1단계: 0초 ~ 10초 미점령 상태
    {
        UpdateAltarColor(NeutralColor); // 하얀색 유지
    }
    else if (SimElapsedTime > 10.0f && SimElapsedTime <= 15.0f) // 2단계: 10초 ~ 15초 레드팀 점령 중
    {
        float Alpha = (SimElapsedTime - 10.0f) / 5.0f; // 5초 동안의 비율 산출
        FLinearColor BlendedColor = FLinearColor::LerpUsingHSV(NeutralColor, RedTeamColor, Alpha); // FVector4를 FLinearColor로 정정
        UpdateAltarColor(BlendedColor); // 머티리얼 갱신
    }
    else if (SimElapsedTime > 10.0f && SimElapsedTime <= 20.0f) // 고정 상태 유지 구간 추가 판정
    {
        UpdateAltarColor(RedTeamColor);
    }
    else if (SimElapsedTime > 20.0f && SimElapsedTime <= 25.0f) // 3단계: 20초 ~ 25초 블루팀 전환 보간
    {
        float Alpha = (SimElapsedTime - 20.0f) / 5.0f; // 5초 보간
        FLinearColor BlendedColor = FLinearColor::LerpUsingHSV(RedTeamColor, BlueTeamColor, Alpha); // FVector4를 FLinearColor로 정정
        UpdateAltarColor(BlendedColor); // 머티리얼 갱신
    }
    else if (SimElapsedTime > 25.0f && SimElapsedTime <= 30.0f) // 4단계: 25초 ~ 30초 점멸 효과 연출 구간
    {
        float TargetAlpha = (FMath::Sin(SimElapsedTime * 10.0f) + 1.0f) * 0.5f; // 사인파를 이용한 깜빡임 계수 추출
        FLinearColor BlendedColor = FLinearColor::LerpUsingHSV(BlueTeamColor, NeutralColor, TargetAlpha * 0.5f); // FVector4를 FLinearColor로 정정
        UpdateAltarColor(BlendedColor); // 머티리얼 갱신
    }
    else // 루프 구간 분기
    {
        SimElapsedTime = 10.0f; // 10초 시점(레드 점령 시작)으로 타이머를 꺾어 루프 구현
    }
}