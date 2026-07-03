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
	PrimaryActorTick.bCanEverTick = true;
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	RootComponent = TurretMesh;
	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));
	TurretTeam = EFTTurretTeam::BlueTeam;
	TurretPosition = EFTTurretPosition::None;
	bIsDying = false;
	DyingTimer = 0.0f;
	MaxDyingTime = 2.0f;
}

UAbilitySystemComponent* AFTTurret::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFTTurret::BeginPlay()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		FGameplayTag TeamTag = (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
		AbilitySystemComponent->AddLooseGameplayTag(TeamTag);
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
	}

	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTurret::OnHealthChanged);
	}
}

void AFTTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDying)
	{
		DyingTimer += DeltaTime;
		if (DyingTimer < MaxDyingTime)
		{
			float DropProgress = DyingTimer / MaxDyingTime;
			FVector NewLocation = InitialLocation;
			NewLocation.Z -= DropProgress * 500.0f;
			NewLocation.X += FMath::RandRange(-5.0f, 5.0f);
			NewLocation.Y += FMath::RandRange(-5.0f, 5.0f);
			SetActorLocation(NewLocation);
		}
		else
		{
			bIsDying = false;
			SetActorHiddenInGame(true);
			SetActorEnableCollision(false);
			SetActorTickEnabled(false);
		}
	}
}

void AFTTurret::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && !bIsDying)
	{
		DebugDestroyTurret();
	}
}

void AFTTurret::DebugDestroyTurret()
{
	if (bIsDying) return;
	bIsDying = true;
	InitialLocation = GetActorLocation();

	if (DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
	}
	if (DestructionEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted);
	}
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->TurretDestroyed(this);
		}
	}
}