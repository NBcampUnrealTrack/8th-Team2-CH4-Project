// Fill out your copyright notice in the Description page of Project Settings.
#include "Level/FTTowerBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"

AFTTowerBase::AFTTowerBase()
{
	PrimaryActorTick.bCanEverTick = false; // 진동 연출 완전 배제 및 최적화를 위해 액터 틱 전면 비활성화

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh")); // 외형 메시 인스턴스 생성
	SetRootComponent(TurretMesh); // 최상위 루트로 설정 완료
	TurretMesh->SetCollisionProfileName(TEXT("BlockAllGeneric")); // 포탑 외형 기본 블록 속성 주입

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox")); // 피격 박스 생성
	CollisionBox->SetupAttachment(TurretMesh); // 외형 메시 아래에 자식으로 연결
	CollisionBox->SetBoxExtent(FVector(100.f, 100.f, 200.f)); // 피격 연산 공간 크기 설정
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 오버랩 감지 전용 주입
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic); // 다이내믹 오브젝트 채널 고정
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap); // 오버랩 응답 전면 동기화
	CollisionBox->SetGenerateOverlapEvents(true); // 이벤트 개방 승인

	DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh")); // 카오스 컴포넌트 생성
	DebrisMesh->SetupAttachment(TurretMesh); // 초기 배치 유지를 위해 루트 하위에 부착

	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent")); // ASC 할당
	AbilitySystemComponent->SetIsReplicated(true); // 복제 개방
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // 미니멀 모드 지정

	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet")); // 스탯 객체 할당
}

UAbilitySystemComponent* AFTTowerBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent; // 상속 인터페이스 ASC 반환
}

void AFTTowerBase::BeginPlay()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // 객체 제어 정보 주입

		const FGameplayTag TeamTag = GetTeamTag(); // 가상 함수 연동 팀 태그 추출
		if (TeamTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(TeamTag); // 인스턴스 태깅
		}

		const FGameplayTag StructTag = GetStructureTag(); // 가상 함수 연동 식별 태그 추출
		if (StructTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(StructTag); // 인스턴스 태깅
		}

		float TargetMaxHealth = GetDefaultMaxHealth(); // 내구도 기본값 로드
		if (TowerDataTable && !DataRowName.IsNone())
		{
			if (const FFTStructureData* RowData = TowerDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
			{
				TargetMaxHealth = RowData->MaxHealth; // 데이터 시트 명세 반영
			}
		}

		if (AttributeSet)
		{
			AttributeSet->InitMaxHealth(TargetMaxHealth); // 명세 속성 기록
			AttributeSet->InitHealth(TargetMaxHealth); // 명세 속성 기록
		}
	}

	Super::BeginPlay(); // 기본 함수 실행

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
			.AddUObject(this, &AFTTowerBase::OnHealthChanged); // 내구도 변동 위임 등록
	}
}

void AFTTowerBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && !bIsDying)
	{
		StartDestruction(); // 내구도 소진 검증 시 파괴 개시
	}
}

void AFTTowerBase::StartDestruction()
{
	if (bIsDying)
	{
		return; // 제어 중복 진입 차단
	}
	bIsDying = true; // 파괴 진행 플래그 승인 고정

	if (DebrisMesh)
	{
		DebrisMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // 종속 차단으로 인한 물리 억제를 방지하기 위해 월드 공간으로 독립 분리
		SetRootComponent(DebrisMesh); // 카오스 파편 메시를 본 액터의 최종 최상위 루트로 강제 승격 교체
		DebrisMesh->SetVisibility(true); // 시각화 개방
		DebrisMesh->SetSimulatePhysics(true); // 제자리 파쇄 연출 가동 승인
	}

	if (TurretMesh)
	{
		TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 구 외형 충돌 전면 배제
		TurretMesh->SetVisibility(false); // 구 외형 시각 즉각 숨김 교체
	}

	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 피격 박스 충돌 완전 무력화
	}

	if (DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 소음 파쇄음 출력
	}

	if (DestructionEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 폭발 이펙트 출력
	}

	NotifyDestroyed(); // 자식 고유 파괴 연동 데이터 전송

	SetLifeSpan(MaxDyingTime); // 5초 후 메모리 정리 루틴 작동
}