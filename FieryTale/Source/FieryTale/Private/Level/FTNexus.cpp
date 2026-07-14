// Copyright Epic Games, Inc. All Rights Reserved.
#include "Level/FTNexus.h"
#include "Components/StaticMeshComponent.h"
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

AFTNexus::AFTNexus()
{
	NexusMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NexusMesh")); // 원본 본진 외형 생성
	SetRootComponent(NexusMesh); // 최상위 루트로 고정
	NexusMesh->SetCollisionProfileName(TEXT("BlockAllGeneric")); // 기본 물리 차단 부여

	DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh")); // 카오스 파편 객체 생성
	DebrisMesh->SetupAttachment(NexusMesh); // 본진 외형 하위에 부착

	NexusTeam = EFTNexusTeam::NoneTeam; // 초기 에러 방지용 진영 할당
}

void AFTNexus::BeginPlay()
{
	float TargetMaxHealth = 5000.0f; // 본진 전용 백업 초기 체력 세팅
	
	if (NexusDataTable && !DataRowName.IsNone())
	{
		if (FFTStructureData* RowData = NexusDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
		{
			TargetMaxHealth = RowData->MaxHealth; // 블루프린트에 등록된 시트 데이터로 체력 수치 완전 덮어쓰기
		}
	}

	if (AttributeSet && AbilitySystemComponent)
	{
		AttributeSet->InitMaxHealth(TargetMaxHealth); // 확보된 시트 체력을 GAS 최대치에 세팅
		AttributeSet->InitHealth(TargetMaxHealth); // 확보된 시트 체력을 GAS 현재치에 세팅
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged); // 공통 권한 개방을 통한 리스너 정상 부착

		if (HasAuthority())
		{
			AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll); // 같은 팀 타워가 하나라도 파괴되기 전까지 넥서스 무적 유지
		}
	}

	Super::BeginPlay(); // 기초 베이스 연동

	if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
	{
		HealthWidgetComp->UpdateASCBinding(); // 넥서스 고유 UI 동기화 강제 지시
	}
}

FGameplayTag AFTNexus::GetTeamTag() const
{
	return (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 설정된 진영 태그 필터링 반환
}

FGameplayTag AFTNexus::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Nexus; // 본진 객체 식별용 고유 태그 반환
}

void AFTNexus::PerformDestructionEffects()
{
	if (AbilitySystemComponent) AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 무력화 태그 삽입

	if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>()) HealthWidgetComp->SetVisibility(false); // 1P/2P 화면에서 체력바 UI 즉각 소거

	if (NexusMesh)
	{
		NexusMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메쉬 겹침 에러 차단을 위해 원본 외형 콜리전 소거
		NexusMesh->SetVisibility(false); // 기존 신전 메쉬 렌더링 전면 은닉
	}

	if (DebrisMesh)
	{
		DebrisMesh->SetVisibility(true); // 숨겨둔 카오스 조각 파편 렌더링 활성화
		DebrisMesh->SetSimulatePhysics(true); // 복구 완료: 캐시 매니저가 조각들을 통제할 수 있도록 카오스 물리 스레드 재개방
	}

	if (DestructionSound) UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 위치 기반 파괴 사운드 재생
	if (DestructionEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 방향 기반 폭발 이펙트 재생

	OnNexusDestroyed(); // 블루프린트단에 캐시 애니메이션 트리거 신호 발송
}

void AFTNexus::SetVulnerable(bool bNewVulnerable)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	if (bNewVulnerable)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

bool AFTNexus::IsVulnerable() const
{
	return AbilitySystemComponent && !AbilitySystemComponent->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invincible);
}

void AFTNexus::NotifyGameMode()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->NexusDestroyed(this); // 오직 1P 서버 환경에서만 매치 종료 룰 연산을 위해 통보 발송
		}
	}
}