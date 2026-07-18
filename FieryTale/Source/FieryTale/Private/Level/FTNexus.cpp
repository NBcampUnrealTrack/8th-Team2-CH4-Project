// Copyright Epic Games, Inc. All Rights Reserved.
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

AFTNexus::AFTNexus()
{
	NexusMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NexusMesh")); // 비주얼 메쉬 컴포넌트 생성
	SetRootComponent(NexusMesh); // 비주얼 메쉬를 루트 컴포넌트로 설정
	NexusMesh->SetCollisionProfileName(TEXT("NoCollision")); // 충돌 간섭을 방지하기 위해 메쉬 자체 콜리전 제거

	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule")); // 물리 충돌 및 라인트레이스용 캡슐 생성
	CollisionCapsule->SetupAttachment(NexusMesh); // 캡슐을 루트 하위에 컴포넌트로 부착
	CollisionCapsule->InitCapsuleSize(300.f, 300.f); // 캡슐 크기를 기획에 맞게 반경과 높이 셋업
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 캡슐의 물리 연산 및 레이캐스트 감지 허용
	CollisionCapsule->SetCollisionProfileName(TEXT("BlockAllGeneric")); // 제네릭 차단 프로필 부여로 정확한 충돌 레이 판정 구현

	DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh")); // 카오스 파편 컴포넌트 생성
	DebrisMesh->SetupAttachment(NexusMesh); // 파편 컴포넌트 부착

	NexusTeam = EFTNexusTeam::NoneTeam; // 진영 정보 안전 기본값 설정
}

void AFTNexus::BeginPlay()
{
	float TargetMaxHealth = 5000.0f; // 최대 체력 기본 폴백 값 할당
	
	if (NexusDataTable && !DataRowName.IsNone())
	{
		if (FFTStructureData* RowData = NexusDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
		{
			TargetMaxHealth = RowData->MaxHealth; // 데이터 테이블 기획 수치 반영
		}
	}

	Super::BeginPlay(); // 부모 초기화 루틴을 선행 호출하여 GAS Actor Info 바인딩 무결성 확보

	if (AttributeSet && AbilitySystemComponent)
	{
		AttributeSet->InitMaxHealth(TargetMaxHealth); // 체력 속성 최대치 동기화 초기화
		AttributeSet->InitHealth(TargetMaxHealth); // 체력 속성 현재치 동기화 초기화
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged); // 체력 상태값 모니터링 변경 감지 이벤트 바인딩

		if (HasAuthority())
		{
			AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll); // 서버 권한 무적 태그 기본 인가
		}
	}

	if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
	{
		HealthWidgetComp->UpdateASCBinding(); // 체력 컴포넌트 전용 위젯 바인딩 갱신 지시
	}
}

FGameplayTag AFTNexus::GetTeamTag() const
{
	return (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 설정 진영을 체크하여 대응하는 전용 태그 반환
}

FGameplayTag AFTNexus::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Nexus; // 본진 객체 구분을 위한 넥서스 태그 반환
}

void AFTNexus::PerformDestructionEffects()
{
	if (AbilitySystemComponent) AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 어빌리티 비활성화를 위한 가동 중지 상태 태그 추가

	if (CollisionCapsule) CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 서버-클라이언트 공통 피격용 캡슐 물리 정지
	if (NexusMesh) NexusMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 서버-클라이언트 공통 물리 장벽 해제

	// 데디케이티드 서버 네트워크 병목 방지를 위한 클라이언트 로컬 전용 렌더 및 시뮬레이션 제어 (포탑과 동일하게 붕괴 현상 수복)
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>()) HealthWidgetComp->SetVisibility(false); // 가시성 소거 처리로 화면 UI 정지
		if (NexusMesh) NexusMesh->SetVisibility(false); // 기존 스태틱 메쉬 렌더링 오프

		if (DebrisMesh)
		{
			DebrisMesh->SetVisibility(true); // 카오스 파편 보이도록 토글
			DebrisMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // 트랜스폼 독립으로 물리 덮어쓰기 현상 차단
			DebrisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 파편 물리 강제 개방
			DebrisMesh->SetCollisionProfileName(TEXT("Destructible")); // 디스트럭터블 반응 프로필 주입
			DebrisMesh->SetSimulatePhysics(true); // 클라이언트 로컬 물리 스레드를 깨워 카오스 붕괴 연출 시뮬레이션 개시
			DebrisMesh->AddRadialImpulse(GetActorLocation(), 1000.0f, 20000.0f, ERadialImpulseFalloff::RIF_Linear, true); // 넥서스 파괴 전용 강제 충격량 주입
		}

		if (DestructionSound) UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 파괴 폭발 사운드 로컬 재생
		if (DestructionEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 가루 이펙트 로컬 스폰

		OnNexusDestroyed(); // 블루프린트 최종 이벤트 재생 로컬 트리거
	}
}

void AFTNexus::SetVulnerable(bool bNewVulnerable)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return; // 네트워크 정합성을 확보하기 위해 권한 검사 및 예외 탈출
	}

	if (bNewVulnerable)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll); // 가해 허가 시 무적 판정 해제
	}
	else
	{
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Buff::Invincible, 1, EGameplayTagReplicationState::TagAndCountToAll); // 가해 비허가 시 무적 태그 덮어쓰기
	}
}

bool AFTNexus::IsVulnerable() const
{
	return AbilitySystemComponent && !AbilitySystemComponent->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invincible); // 무적 상태 태그 소유 상태가 아니면 true 반환
}

void AFTNexus::NotifyGameMode()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->NexusDestroyed(this); // 서버 게임모드에 넥서스 파괴 통보
		}
	}
	SetLifeSpan(5.0f); // 충돌 찌꺼기가 남아있는 고스트 액터 버그 방지를 위한 생명 주기 자동 강제 제한
}