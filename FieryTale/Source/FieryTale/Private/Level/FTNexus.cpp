#include "Level/FTNexus.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "Core/Game/FTGameMode.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// 넥서스 생성 및 컴포넌트 초기화
AFTNexus::AFTNexus()
{
    NexusMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NexusMesh"));
    SetRootComponent(NexusMesh);
    NexusMesh->SetCollisionProfileName(TEXT("NoCollision"));

    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    CollisionCapsule->SetupAttachment(NexusMesh);
    CollisionCapsule->InitCapsuleSize(300.f, 300.f);
    CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionCapsule->SetCollisionProfileName(TEXT("BlockAllGeneric"));

    DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh"));
    DebrisMesh->SetupAttachment(NexusMesh);

    NexusTeam = EFTNexusTeam::NoneTeam;
}

// 넥서스 시작 설정 및 무적 버프 부여
void AFTNexus::BeginPlay()
{
    float TargetMaxHealth = 5000.0f;

    if (NexusDataTable && !DataRowName.IsNone())
    {
        if (FFTStructureData* RowData = NexusDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
        {
            TargetMaxHealth = RowData->MaxHealth;
        }
    }

    Super::BeginPlay();

    if (AttributeSet && AbilitySystemComponent)
    {
        AttributeSet->InitMaxHealth(TargetMaxHealth);
        AttributeSet->InitHealth(TargetMaxHealth);
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged);

        if (HasAuthority())
        {
            AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll);
        }
    }

    if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
    {
        HealthWidgetComp->UpdateASCBinding();
    }
}

// 넥서스 팀 태그 반환
FGameplayTag AFTNexus::GetTeamTag() const
{
    return (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
}

// 넥서스 구조물 태그 반환
FGameplayTag AFTNexus::GetStructureTag() const
{
    return FTTags::FTObjectType::Structure_Nexus;
}

// 파괴 시각 효과 처리 및 물리 시뮬레이션 활성화
void AFTNexus::PerformDestructionEffects()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted);
    }

    if (CollisionCapsule)
    {
        CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (NexusMesh)
    {
        NexusMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
        {
            HealthWidgetComp->SetVisibility(false);
        }

        if (NexusMesh)
        {
            NexusMesh->SetVisibility(false);
        }

        if (DebrisMesh)
        {
            DebrisMesh->SetVisibility(true);
            DebrisMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
            DebrisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            DebrisMesh->SetCollisionProfileName(TEXT("Destructible"));
            DebrisMesh->SetSimulatePhysics(true);
            DebrisMesh->AddRadialImpulse(GetActorLocation(), 1000.0f, 20000.0f, ERadialImpulseFalloff::RIF_Linear, true);
        }

        if (DestructionSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
        }

        if (DestructionEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());
        }
    }

    OnNexusDestroyed();
}

// 넥서스 취약 상태 설정
void AFTNexus::SetVulnerable(bool bNewVulnerable)
{
    if (!AbilitySystemComponent || !HasAuthority()) return;

    if (bNewVulnerable)
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
    else
    {
        AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
}

// 넥서스 취약 여부 확인
bool AFTNexus::IsVulnerable() const
{
    return AbilitySystemComponent && !AbilitySystemComponent->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invincible);
}

// 게임 모드에 넥서스 파괴 전달
void AFTNexus::NotifyGameMode()
{
    if (UWorld* World = GetWorld())
    {
        if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
        {
            GameMode->NexusDestroyed(this);
        }
    }
    SetLifeSpan(5.0f);
}