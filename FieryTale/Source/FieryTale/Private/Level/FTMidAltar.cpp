#include "Level/FTMidAltar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Character/FTPlayerState.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFTMidAltar::AFTMidAltar()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;

    AltarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AltarMesh"));
    RootComponent = AltarMesh;

    HologramRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HologramRingMesh"));
    HologramRingMesh->SetupAttachment(RootComponent);
    HologramRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HologramRingMesh->SetCollisionProfileName(TEXT("NoCollision"));

    CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume"));
    CaptureVolume->SetupAttachment(RootComponent);
    CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CaptureVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    CaptureVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    CaptureAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CaptureAudioComponent"));
    CaptureAudioComponent->SetupAttachment(RootComponent);
    CaptureAudioComponent->bAutoActivate = false;

    AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    AltarTeam = EFTAltarTeam::None;
    CurrentControllingTeam = EFTAltarTeam::None;
    CurrentState = EFTAltarState::Neutral;
    PreviousState = EFTAltarState::Neutral;
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

void AFTMidAltar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (HasAuthority())
    {
        ProcessCaptureRules(DeltaTime);
    }
    UpdateAltarVisuals();
}

UAbilitySystemComponent* AFTMidAltar::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AFTMidAltar::SetGenericTeamId(const FGenericTeamId& InTeamID)
{
    TeamId = InTeamID;
}

FGenericTeamId AFTMidAltar::GetGenericTeamId() const
{
    return TeamId;
}

void AFTMidAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFTMidAltar, CurrentState);
    DOREPLIFETIME(AFTMidAltar, AltarTeam);
    DOREPLIFETIME(AFTMidAltar, CurrentControllingTeam);
    DOREPLIFETIME(AFTMidAltar, CaptureProgress);
    DOREPLIFETIME(AFTMidAltar, RemainingLockTime);
}

void AFTMidAltar::OpenAltar()
{
    if (!HasAuthority()) return;
    AltarTeam = EFTAltarTeam::None;
    CurrentControllingTeam = EFTAltarTeam::None;
    CurrentState = EFTAltarState::Neutral;
    CaptureProgress = 0.0f;
    RemainingLockTime = 0.0f;
    UpdateAltarVisuals();
    
    SetActorTickEnabled(false);
}

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
    }
    AttributeSet->InitMaxHealth(10000.0f);
    AttributeSet->InitHealth(10000.0f);
    
    PreviousState = CurrentState;
    UpdateAltarVisuals();

    if (CurrentState == EFTAltarState::Neutral)
    {
        SetActorTickEnabled(false);
    }
}

void AFTMidAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;
    bool bValid = false;
    GetCharacterTeam(OtherActor, bValid);
    if (OtherActor && OtherActor != this && bValid)
    {
        OverlappingPlayers.AddUnique(OtherActor);
        SetActorTickEnabled(true);
    }
}

void AFTMidAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;
    if (OtherActor)
    {
        OverlappingPlayers.Remove(OtherActor);
    }
}

void AFTMidAltar::ProcessCaptureRules(float DeltaTime)
{
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

        if (CurrentControllingTeam != EFTAltarTeam::None)
        {
            CurrentState = EFTAltarState::Decaying;
            // 플레이어가 없을 때(자연 감소) 잃는 진행 속도를 반으로 줄임
            CaptureProgress -= (DeltaTime / MaxCaptureTime) * (CaptureDecayMultiplier * 0.5f);
            if (CaptureProgress <= 0.0f)
            {
                CaptureProgress = 0.0f;
                CurrentControllingTeam = EFTAltarTeam::None;
                CurrentState = EFTAltarState::Neutral;
                SetActorTickEnabled(false);
            }
        }
        return;
    }

    EFTAltarTeam ActiveTeam = (BlueTeamCount > 0) ? EFTAltarTeam::BlueTeam : EFTAltarTeam::RedTeam;
    CurrentState = EFTAltarState::Capturing;

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
        CurrentState = EFTAltarState::Decaying;
        // 적 팀이 진입하여 탈취할 때는 기존 감소 속도를 유지함
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

void AFTMidAltar::UpdateAltarVisuals()
{
    FLinearColor MatColor = NeutralColor;

    if (CurrentState == EFTAltarState::Locked)
    {
        MatColor = NeutralColor;
    }
    else if (CurrentControllingTeam == EFTAltarTeam::BlueTeam)
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

    if (CurrentState == EFTAltarState::Capturing)
    {
        if (!CaptureAudioComponent->IsPlaying() && CaptureLoopSound)
        {
            CaptureAudioComponent->SetSound(CaptureLoopSound);
            
            float StartTime = CaptureProgress * MaxCaptureTime;
            CaptureAudioComponent->Play(StartTime);
        }
    }
    else
    {
        if (CaptureAudioComponent->IsPlaying())
        {
            CaptureAudioComponent->Stop();
        }
    }

    if (PreviousState != EFTAltarState::Locked && CurrentState == EFTAltarState::Locked)
    {
        if (CaptureSuccessSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, CaptureSuccessSound, GetActorLocation());
        }
    }
    
    PreviousState = CurrentState;
}

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

void AFTMidAltar::OnRep_CaptureData()
{
    if (CurrentState == EFTAltarState::Neutral)
    {
        SetActorTickEnabled(false);
    }
    else
    {
        SetActorTickEnabled(true);
    }
    
    UpdateAltarVisuals();
}