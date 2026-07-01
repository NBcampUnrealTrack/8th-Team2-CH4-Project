#include "Level/FTNexus.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Core/Game/FTGameMode.h"
#include "GameplayEffectTypes.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AFTNexus::AFTNexus()
{
	PrimaryActorTick.bCanEverTick = true;

	NexusMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NexusMesh"));
	RootComponent = NexusMesh;

	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

	NexusTeam = EFTNexusTeam::None;
	bIsDying = false;
	DyingTimer = 0.0f;
	MaxDyingTime = 2.0f;
}

UAbilitySystemComponent* AFTNexus::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFTNexus::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		FGameplayTag TeamTag = (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
		AbilitySystemComponent->AddLooseGameplayTag(TeamTag);
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTObjectType::Structure_Nexus);

		float TargetMaxHealth = 5000.0f;
		if (NexusDataTable && !DataRowName.IsNone())
		{
			FFTStructureData* RowData = NexusDataTable->FindRow<FFTStructureData>(DataRowName, TEXT(""));
			if (RowData)
			{
				TargetMaxHealth = RowData->MaxHealth;
			}
		}

		AttributeSet->InitMaxHealth(TargetMaxHealth);
		AttributeSet->InitHealth(TargetMaxHealth);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTNexus::OnHealthChanged);
	}

	GetWorldTimerManager().SetTimer(AutoDestroyTimerHandle, this, &AFTNexus::DebugDestroyNexus, 30.0f, false);
}

void AFTNexus::Tick(float DeltaTime)
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

void AFTNexus::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && !bIsDying)
	{
		DebugDestroyNexus();
	}
}

void AFTNexus::DebugDestroyNexus()
{
	if (bIsDying) return;

	bIsDying = true;
	InitialLocation = GetActorLocation();

	GetWorldTimerManager().ClearTimer(AutoDestroyTimerHandle);

	if (DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
	}

	if (DestructionEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());
	}

	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->NexusDestroyed(this);
		}
	}
}