// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/FTTowerBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"

AFTTowerBase::AFTTowerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	//	루트를 박스 콜리전으로. 투사체는 OverlapAllDynamic(동적만 오버랩, WorldStatic 무시)이므로
	//	박스를 WorldDynamic + 전 채널 오버랩 + 오버랩 이벤트 생성으로 두어야 투사체가 실제로 오버랩된다.
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(100.f, 100.f, 200.f)); // 기본값 — BP/인스턴스에서 실제 크기에 맞게 조정
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);

	//	외형 메시는 루트 박스에 부착. 콜리전은 기본값(이동 차단)을 유지하되 필요 시 BP에서 조정.
	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	TowerMesh->SetupAttachment(CollisionBox);

	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AFTTowerBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFTTowerBase::BeginPlay()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		const FGameplayTag TeamTag = GetTeamTag();
		if (TeamTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(TeamTag);
		}

		const FGameplayTag StructTag = GetStructureTag();
		if (StructTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(StructTag);
		}

		float TargetMaxHealth = GetDefaultMaxHealth();
		if (TowerDataTable && !DataRowName.IsNone())
		{
			if (const FFTStructureData* RowData = TowerDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
			{
				TargetMaxHealth = RowData->MaxHealth;
			}
		}

		if (AttributeSet)
		{
			AttributeSet->InitMaxHealth(TargetMaxHealth);
			AttributeSet->InitHealth(TargetMaxHealth);
		}
	}

	//	체력 초기화가 끝난 뒤 부모 BeginPlay(위젯 컴포넌트 등)를 호출해야 체력바가 올바른 초기값을 읽는다.
	Super::BeginPlay();

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
			.AddUObject(this, &AFTTowerBase::OnHealthChanged);
	}
}

void AFTTowerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDying)
	{
		DyingTimer += DeltaTime;
		if (DyingTimer < MaxDyingTime)
		{
			const float DropProgress = DyingTimer / MaxDyingTime;
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

void AFTTowerBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && !bIsDying)
	{
		StartDestruction();
	}
}

void AFTTowerBase::StartDestruction()
{
	if (bIsDying)
	{
		return;
	}
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

	NotifyDestroyed();
}
