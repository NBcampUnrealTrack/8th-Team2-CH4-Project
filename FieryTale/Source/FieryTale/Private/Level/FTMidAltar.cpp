#include "Level/FTMidAltar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Character/FTPlayerState.h"
#include "GameplayEffect.h"

AFTMidAltar::AFTMidAltar()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    AltarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AltarMesh"));
    RootComponent = AltarMesh;

    HologramRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HologramRingMesh"));
    HologramRingMesh->SetupAttachment(RootComponent);
    HologramRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume"));
    CaptureVolume->SetupAttachment(RootComponent);
    CaptureVolume->SetCollisionProfileName(TEXT("Trigger"));

    AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    AltarTeam = EFTAltarTeam::None;
    CurrentControllingTeam = EFTAltarTeam::None;
    CurrentState = EFTAltarState::Neutral;
    CaptureProgress = 0.0f;
    RemainingLockTime = 0.0f;

    TeamColorParameterName = TEXT("TeamColor");
    ProgressParameterName = TEXT("Progress");
    ContestedParameterName = TEXT("IsContested");
    
    NeutralColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);
    BlueTeamColor = FLinearColor(0.0f, 0.2f, 1.0f, 1.0f);
    RedTeamColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);

    MaxCaptureTime = 8.0f;
    CaptureDecayMultiplier = 1.5f;
    LockDuration = 60.0f;
    CaptureBuffClass = nullptr;
}

// GAS 인터페이스 상속 구조 반환 함수입니다.
UAbilitySystemComponent* AFTMidAltar::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// 피아식별 팀 ID를 등록합니다.
void AFTMidAltar::SetGenericTeamId(const FGenericTeamId& InTeamID)
{
    TeamId = InTeamID;
}

// 내부 팀 ID 값을 반환합니다.
FGenericTeamId AFTMidAltar::GetGenericTeamId() const
{
    return TeamId;
}

// 네트워크 멤버 변수들을 동기화 목록에 추가합니다.
void AFTMidAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AFTMidAltar, CurrentState);
    DOREPLIFETIME(AFTMidAltar, AltarTeam);
    DOREPLIFETIME(AFTMidAltar, CurrentControllingTeam);
    DOREPLIFETIME(AFTMidAltar, CaptureProgress);
    DOREPLIFETIME(AFTMidAltar, RemainingLockTime);
}

// 거점 잠금이 끝났거나 강제 오픈 시 완전 초기화 상태로 환원합니다.
void AFTMidAltar::OpenAltar()
{
    if (!HasAuthority()) return;

    AltarTeam = EFTAltarTeam::None;
    CurrentControllingTeam = EFTAltarTeam::None;
    CurrentState = EFTAltarState::Neutral;
    CaptureProgress = 0.0f;
    RemainingLockTime = 0.0f;

    UpdateAltarVisuals();
}

// 초기 다이내믹 머티리얼 캐싱 및 겹침 이벤트를 바인딩합니다.
void AFTMidAltar::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CaptureVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTMidAltar::OnOverlapBegin);
        CaptureVolume->OnComponentEndOverlap.AddDynamic(this, &AFTMidAltar::OnOverlapEnd);
    }

    if (AltarMesh)
    {
        DynamicAltarMaterial = AltarMesh->CreateDynamicMaterialInstance(0);
    }

    if (HologramRingMesh)
    {
        DynamicHologramMaterial = HologramRingMesh->CreateDynamicMaterialInstance(0);
    }

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        AttributeSet->InitMaxHealth(10000.0f);
        AttributeSet->InitHealth(10000.0f);
    }

    UpdateAltarVisuals();
}

// 서버 권한 검증 하에 실시간 잠금 수치 및 역점령 연산을 분기 처리합니다.
void AFTMidAltar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        ProcessCaptureRules(DeltaTime);
    }

    UpdateAltarVisuals();
}

// 진입 액터의 팀 정보 유효성을 확인하고 임시 적재합니다.
void AFTMidAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    bool bValid = false;
    GetCharacterTeam(OtherActor, bValid);
    if (OtherActor && OtherActor != this && bValid)
    {
        OverlappingPlayers.AddUnique(OtherActor);
    }
}

// 감지 영역을 벗어난 액터를 즉시 배열에서 지워냅니다.
void AFTMidAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;

    if (OtherActor)
    {
        OverlappingPlayers.Remove(OtherActor);
    }
}

// 60초 잠금 분기와 오버워치 공수교대 및 실시간 버프 부여 연산을 정밀 처리합니다.
void AFTMidAltar::ProcessCaptureRules(float DeltaTime)
{
    // 잠금 상태인 경우, 플레이어 검출을 완전히 무시하고 타이머 감소 및 진행도를 차감합니다.
    if (CurrentState == EFTAltarState::Locked)
    {
        RemainingLockTime -= DeltaTime;
        CaptureProgress = FMath::Clamp(RemainingLockTime / LockDuration, 0.0f, 1.0f);
        if (RemainingLockTime <= 0.0f)
        {
            OpenAltar();
        }
        return;
    }

    int32 BlueTeamCount = 0;
    int32 RedTeamCount = 0;

    for (int32 i = OverlappingPlayers.Num() - 1; i >= 0; --i)
    {
        AActor* Actor = OverlappingPlayers[i];
        if (!IsValid(Actor))
        {
            OverlappingPlayers.RemoveAt(i);
            continue;
        }

        bool bValid = false;
        EFTTeam Team = GetCharacterTeam(Actor, bValid);
        if (bValid)
        {
            if (Team == EFTTeam::Blue) BlueTeamCount++;
            else if (Team == EFTTeam::Red) RedTeamCount++;
        }
    }

    if (BlueTeamCount > 0 && RedTeamCount > 0)
    {
        CurrentState = EFTAltarState::Contested;
        return;
    }

    bool bHasPlayers = (BlueTeamCount > 0 || RedTeamCount > 0);
    
    if (!bHasPlayers)
    {
        if (CurrentState == EFTAltarState::Contested)
        {
            CurrentState = (AltarTeam == EFTAltarTeam::None) ? EFTAltarState::Neutral : EFTAltarState::Captured;
        }
        return;
    }

    EFTAltarTeam ActiveTeam = (BlueTeamCount > 0) ? EFTAltarTeam::BlueTeam : EFTAltarTeam::RedTeam;
    CurrentState = EFTAltarState::Progressing;

    if (CurrentControllingTeam == EFTAltarTeam::None)
    {
        CurrentControllingTeam = ActiveTeam;
        CaptureProgress += DeltaTime / MaxCaptureTime;
    }
    else if (CurrentControllingTeam == ActiveTeam)
    {
        CaptureProgress += DeltaTime / MaxCaptureTime;
        if (CaptureProgress >= 1.0f)
        {
            CaptureProgress = 1.0f;
            AltarTeam = ActiveTeam;
            CurrentState = EFTAltarState::Locked;
            RemainingLockTime = LockDuration;

            // 점령 성공 시점에 범위 내에 존재하는 아군 플레이어들에게 GAS 버프를 즉각 분산 부여합니다.
            if (CaptureBuffClass)
            {
                EFTTeam WinningTeam = (AltarTeam == EFTAltarTeam::BlueTeam) ? EFTTeam::Blue : EFTTeam::Red;
                for (AActor* Actor : OverlappingPlayers)
                {
                    bool bValidTeam = false;
                    EFTTeam Team = GetCharacterTeam(Actor, bValidTeam);
                    if (bValidTeam && Team == WinningTeam)
                    {
                        APawn* Pawn = Cast<APawn>(Actor);
                        if (Pawn && Pawn->GetPlayerState())
                        {
                            AFTPlayerState* FTPS = Cast<AFTPlayerState>(Pawn->GetPlayerState());
                            if (FTPS && FTPS->GetAbilitySystemComponent())
                            {
                                UAbilitySystemComponent* ASC = FTPS->GetAbilitySystemComponent();
                                FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
                                Context.AddSourceObject(this);
                                FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CaptureBuffClass, 1.0f, Context);
                                if (Spec.IsValid())
                                {
                                    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        CaptureProgress -= (DeltaTime / MaxCaptureTime) * CaptureDecayMultiplier;
        if (CaptureProgress <= 0.0f)
        {
            CaptureProgress = 0.0f;
            CurrentControllingTeam = ActiveTeam;
            AltarTeam = EFTAltarTeam::None;
        }
    }

    CaptureProgress = FMath::Clamp(CaptureProgress, 0.0f, 1.0f);
}

// 도출된 내부 변수 데이터를 각 다이내믹 머티리얼 인스턴스에 밀어넣습니다.
void AFTMidAltar::UpdateAltarVisuals()
{
    FLinearColor MatColor = NeutralColor;

    // 잠금 상태인 경우, 점령 수치는 회색 원형 시계 게이지 연출로 통일 표현됩니다.
    if (CurrentState == EFTAltarState::Locked)
    {
        MatColor = NeutralColor;
    }
    else
    {
        if (CurrentControllingTeam == EFTAltarTeam::BlueTeam)
        {
            MatColor = BlueTeamColor;
        }
        else if (CurrentControllingTeam == EFTAltarTeam::RedTeam)
        {
            MatColor = RedTeamColor;
        }

        if (CurrentState == EFTAltarState::Captured)
        {
            MatColor = (AltarTeam == EFTAltarTeam::BlueTeam) ? BlueTeamColor : RedTeamColor;
        }
    }

    float IsContestedValue = (CurrentState == EFTAltarState::Contested) ? 1.0f : 0.0f;

    if (DynamicAltarMaterial)
    {
        DynamicAltarMaterial->SetVectorParameterValue(TeamColorParameterName, MatColor);
        DynamicAltarMaterial->SetScalarParameterValue(ProgressParameterName, CaptureProgress);
        DynamicAltarMaterial->SetScalarParameterValue(ContestedParameterName, IsContestedValue);
    }

    if (DynamicHologramMaterial)
    {
        DynamicHologramMaterial->SetVectorParameterValue(TeamColorParameterName, MatColor);
        DynamicHologramMaterial->SetScalarParameterValue(ProgressParameterName, CaptureProgress);
        DynamicHologramMaterial->SetScalarParameterValue(ContestedParameterName, IsContestedValue);
    }
}

// 폰의 상태로부터 실제 소속 블루/레드 팀을 구별하는 함수입니다.
EFTTeam AFTMidAltar::GetCharacterTeam(AActor* Actor, bool& bIsValid) const
{
    bIsValid = false;
    APawn* Pawn = Cast<APawn>(Actor);
    if (!Pawn) return EFTTeam::Blue;

    APlayerState* PS = Pawn->GetPlayerState();
    if (!PS) return EFTTeam::Blue;

    AFTPlayerState* FTPS = Cast<AFTPlayerState>(PS);
    if (!FTPS) return EFTTeam::Blue;

    bIsValid = true;
    return FTPS->GetTeam();
}

// 네트워크 통신에 응해 클라이언트 뷰의 비주얼 렌더링을 갱신합니다.
void AFTMidAltar::OnRep_CaptureData()
{
    UpdateAltarVisuals();
}