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

AFTMidAltar::AFTMidAltar()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    AltarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AltarMesh"));
    RootComponent = AltarMesh;

    HologramRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HologramRingMesh"));
    HologramRingMesh->SetupAttachment(RootComponent);
    HologramRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // USphereComponent 타입으로 형 변환 불일치가 없도록 정상 생성합니다.
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

    TeamColorParameterName = TEXT("TeamColor");
    ProgressParameterName = TEXT("Progress");
    ContestedParameterName = TEXT("IsContested");
    
    NeutralColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);
    BlueTeamColor = FLinearColor(0.0f, 0.2f, 1.0f, 1.0f);
    RedTeamColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);

    MaxCaptureTime = 8.0f;
    CaptureDecayMultiplier = 1.5f;
}

// 폰의 컴포넌트 정보 및 팀 정보를 가져오는 함수입니다.
UAbilitySystemComponent* AFTMidAltar::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// 피아식별 팀 ID를 변경합니다.
void AFTMidAltar::SetGenericTeamId(const FGenericTeamId& InTeamID)
{
    TeamId = InTeamID;
}

// 내부 팀 ID 값을 외부에 리턴합니다.
FGenericTeamId AFTMidAltar::GetGenericTeamId() const
{
    return TeamId;
}

// 네트워크 멤버 변수를 동기화 세트에 등록합니다.
void AFTMidAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AFTMidAltar, CurrentState);
    DOREPLIFETIME(AFTMidAltar, AltarTeam);
    DOREPLIFETIME(AFTMidAltar, CurrentControllingTeam);
    DOREPLIFETIME(AFTMidAltar, CaptureProgress);
}

// 다음 매치 재개방 타이밍에 회색 상태로 되돌리고 잠금을 해제합니다.
void AFTMidAltar::OpenAltar()
{
    if (!HasAuthority()) return;

    AltarTeam = EFTAltarTeam::None;
    CurrentControllingTeam = EFTAltarTeam::None;
    CurrentState = EFTAltarState::Neutral;
    CaptureProgress = 0.0f;

    UpdateAltarVisuals();
}

// 초기화 연동 및 클라이언트 바닥/공중용 다이내믹 머티리얼을 캐싱 등록합니다.
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

// 매 틱 연산하며, Locked가 아닐 때 점령도 데이터 판별 및 동적 보간을 수행합니다.
void AFTMidAltar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && CurrentState != EFTAltarState::Locked)
    {
        ProcessCaptureRules(DeltaTime);
    }

    UpdateAltarVisuals();
}

// 구역 감지 시 소속 팀 정보를 안전히 검증한 액터들만 배열에 등록합니다.
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

// 구역 이탈 발생 감지 시 인스턴스를 관리 대열 목록에서 완전히 소거합니다.
void AFTMidAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;

    if (OtherActor)
    {
        OverlappingPlayers.Remove(OtherActor);
    }
}

// 오버워치 룰셋(밀려났을 시 감소, 경쟁 시 대기, 충돌 구역 퇴장 시 상태 유지)을 직접 판정합니다.
void AFTMidAltar::ProcessCaptureRules(float DeltaTime)
{
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
            CurrentState = EFTAltarState::Progressing;
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
        }
    }
    else
    {
        CaptureProgress -= (DeltaTime / MaxCaptureTime) * CaptureDecayMultiplier;
        if (CaptureProgress <= 0.0f)
        {
            CaptureProgress = 0.0f;
            CurrentControllingTeam = ActiveTeam;
        }
    }

    CaptureProgress = FMath::Clamp(CaptureProgress, 0.0f, 1.0f);
}

// 실시간 상태 값들을 바닥과 공중 장막 머티리얼 인스턴스에 적용합니다.
void AFTMidAltar::UpdateAltarVisuals()
{
    FLinearColor MatColor = NeutralColor;
    if (CurrentControllingTeam == EFTAltarTeam::BlueTeam)
    {
        MatColor = BlueTeamColor;
    }
    else if (CurrentControllingTeam == EFTAltarTeam::RedTeam)
    {
        MatColor = RedTeamColor;
    }

    if (CurrentState == EFTAltarState::Locked)
    {
        MatColor = (AltarTeam == EFTAltarTeam::BlueTeam) ? BlueTeamColor : RedTeamColor;
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

// 폰의 소속을 판단하여 Blue 혹은 Red EFTTeam 형태로 반정형 매핑해 리턴합니다.
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

// 네트워크 통신 수신 즉시 클라이언트 렌더링 동적 머티리얼을 업데이트합니다.
void AFTMidAltar::OnRep_CaptureData()
{
    UpdateAltarVisuals();
}