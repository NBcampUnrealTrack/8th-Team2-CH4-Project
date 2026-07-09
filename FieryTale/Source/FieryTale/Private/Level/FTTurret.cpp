// Copyright Epic Games, Inc. All Rights Reserved.
#include "Level/FTTurret.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Object/FT_ProjectileBase.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "Core/Game/FTGameMode.h"
#include "Engine/World.h"

AFTTurret::AFTTurret()
{
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh")); // 원본 포탑 외형 생성
	SetRootComponent(TurretMesh); // 최상위 루트로 고정
	TurretMesh->SetCollisionProfileName(TEXT("BlockAllGeneric")); // 기본 물리 차단 부여

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox")); // 타격 판정 박스 생성
	CollisionBox->SetupAttachment(TurretMesh); // 타워 외형에 부착
	CollisionBox->SetBoxExtent(FVector(100.f, 100.f, 200.f)); // 박스 부피 셋업
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 오버랩 전용 활성화
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic); // 판정 채널 셋업
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap); // 겹침 감지 셋업
	CollisionBox->SetGenerateOverlapEvents(true); // 이벤트 개방

	LaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserMesh")); // 빔 연출 객체 생성
	LaserMesh->SetupAttachment(TurretMesh); // 타워 하위 부착
	LaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 빔 물리 차단
	LaserMesh->SetUsingAbsoluteLocation(true); // 오차 방지용 좌표계 독립
	LaserMesh->SetUsingAbsoluteRotation(true); // 오차 방지용 회전계 독립
	LaserMesh->SetUsingAbsoluteScale(true); // 오차 방지용 비율계 독립

	DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh")); // 카오스 파편 객체 생성
	DebrisMesh->SetupAttachment(TurretMesh); // 타워 하위 부착

	TurretTeam = EFTTurretTeam::BlueTeam; // 초기 진영 할당
	TurretPosition = EFTTurretPosition::NonePosition; // 초기 라인 할당

	DetectionRange = 1200.0f; // 사거리 변수 초기화
	AttackCooldown = 1.5f; // 사격 주기 초기화
	CurrentTarget = nullptr; // 타겟 소거
	bCanAttack = true; // 사격 가용 개방
}

void AFTTurret::BeginPlay()
{
	float TargetMaxHealth = 1000.0f; // 백업용 초기 체력 설정
	
	if (TurretDataTable && !DataRowName.IsNone())
	{
		if (FFTStructureData* RowData = TurretDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
		{
			TargetMaxHealth = RowData->MaxHealth; // 블루프린트에 등록된 시트 데이터로 체력 수치 완전 덮어쓰기
		}
	}

	if (AttributeSet && AbilitySystemComponent)
	{
		AttributeSet->InitMaxHealth(TargetMaxHealth); // 확보된 시트 체력을 GAS 최대치에 세팅
		AttributeSet->InitHealth(TargetMaxHealth); // 확보된 시트 체력을 GAS 현재치에 세팅
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged); // 개방된 public 권한으로 체력 리스너 부착 완료
	}

	Super::BeginPlay(); // 기초 베이스 연동

	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		if (UFTHealthWidgetComponent* FTHealthComp = Cast<UFTHealthWidgetComponent>(HealthWidgetComp))
		{
			FTHealthComp->UpdateASCBinding(); // 포탑 고유 UI 흰색 마비 버그 방지용 수동 바인딩
		}
	}

	if (LaserMesh) LaserMesh->SetVisibility(false); // 시작 시 타겟 부재로 인한 조준선 즉각 숨김

	GetWorld()->GetTimerManager().SetTimer(TargetTrackingTimerHandle, this, &AFTTurret::TrackNearestEnemy, 0.03f, true); // 탐지 루프 0.03초 주기 가동
}

FGameplayTag AFTTurret::GetTeamTag() const
{
	return (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 에디터 지정 진영에 맞는 태그 필터링 반환
}

FGameplayTag AFTTurret::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Turret; // 포탑 객체 식별용 고유 태그 반환
}

void AFTTurret::PerformDestructionEffects()
{
	if (AbilitySystemComponent) AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 무적 판정 및 무력화 태그 삽입

	GetWorld()->GetTimerManager().ClearTimer(TargetTrackingTimerHandle); // 포탑 감시 루프 영구 소거
	GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle); // 포탑 연사 루프 영구 소거

	if (LaserMesh) LaserMesh->SetVisibility(false); // 1P/2P 화면에서 붉은 조준선 즉각 삭제
	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>()) HealthWidgetComp->SetVisibility(false); // 1P/2P 화면에서 체력바 UI 즉각 소거

	if (TurretMesh)
	{
		TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메쉬 겹침 에러 차단을 위해 원본 외형 콜리전 소거
		TurretMesh->SetVisibility(false); // 기존 기둥 메쉬 렌더링 전면 은닉
	}

	if (CollisionBox) CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 피격 박스 판정 정지

	if (DebrisMesh)
	{
		DebrisMesh->SetVisibility(true); // 숨겨둔 카오스 조각 파편 렌더링 활성화
		DebrisMesh->SetSimulatePhysics(true); // 언리얼 물리 엔진에 파편 시뮬레이션 계산 개방 지시
	}

	if (DestructionSound) UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 위치 기반 파괴 사운드 재생
	if (DestructionEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 방향 기반 폭발 이펙트 재생

	OnTurretDestroyed(); // 1P와 2P의 블루프린트 그래프로 파괴 신호를 발송하여 카오스 매니저 노드 동시 가동
}

void AFTTurret::NotifyGameMode()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->TurretDestroyed(this); // 오직 1P 서버 환경에서만 게임 룰 연산을 위해 통보 발송
		}
	}
	SetLifeSpan(5.0f); // 미선언 오류를 해결하기 위해 상수로 서버 지시 하 메모리 완전 소거 지정
}

void AFTTurret::TrackNearestEnemy()
{
	if (bIsDying) return; // 파괴 시 스레드 점유 중지

	if (CurrentTarget && (GetDistanceTo(CurrentTarget) > DetectionRange))
	{
		CurrentTarget = nullptr; // 사거리 이탈 즉각 포인터 해제
	}

	if (CurrentTarget)
	{
		AFTPlayerCharacterBase* TargetChar = Cast<AFTPlayerCharacterBase>(CurrentTarget);
		if (TargetChar && TargetChar->GetAbilitySystemComponent())
		{
			if (TargetChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
			{
				CurrentTarget = nullptr; // 사망한 캐릭터 포인터 해제
			}
		}
	}

	if (!CurrentTarget)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFTPlayerCharacterBase::StaticClass(), FoundActors); // 맵 전역 캐릭터 색인
		
		AActor* NearestEnemy = nullptr;
		float ClosestDistance = DetectionRange;

		for (AActor* Actor : FoundActors)
		{
			AFTPlayerCharacterBase* EnemyChar = Cast<AFTPlayerCharacterBase>(Actor); // 캐릭터 타입 일치 검사
			if (!EnemyChar) continue;

			UAbilitySystemComponent* TargetASC = EnemyChar->GetAbilitySystemComponent(); // 대상 ASC 유효성 검사
			if (!TargetASC || TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue; // 사망자 색인 배제

			bool bIsEnemy = false;
			if (TurretTeam == EFTTurretTeam::BlueTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsEnemy = true;
			if (TurretTeam == EFTTurretTeam::RedTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsEnemy = true;

			if (bIsEnemy)
			{
				float Distance = GetDistanceTo(EnemyChar); // 타겟과의 거리 검증 연산
				if (Distance < ClosestDistance)
				{
					ClosestDistance = Distance; // 최단 거리 갱신
					NearestEnemy = EnemyChar; // 새 표적 주소 바인딩
				}
			}
		}
		CurrentTarget = NearestEnemy; // 최종 표적 확정
	}

	if (CurrentTarget && LaserMesh)
	{
		LaserMesh->SetVisibility(true); // 조준선 활성화

		FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 포탑 머리 원점 연산
		FVector EndLocation = CurrentTarget->GetActorLocation(); // 표적 중심 도달점 연산
		
		FVector MidLocation = (StartLocation + EndLocation) * 0.5f; // 메쉬 중심 기준점 교정 좌표 연산
		float Distance = FVector::Distance(StartLocation, EndLocation); // 메쉬 길이 스케일링용 거리 추출
		FRotator LaserRot = UKismetMathLibrary::FindLookAtRotation(StartLocation, EndLocation); // 포탑-표적 간 각도 추출
		
		LaserMesh->SetWorldLocation(MidLocation); // 중앙값 이송
		LaserMesh->SetWorldRotation(LaserRot); // 표적 회전 정렬
		LaserMesh->SetWorldScale3D(FVector(Distance / 100.0f, 0.05f, 0.05f)); // 배율 증축 정렬

		if (bCanAttack && HasAuthority()) // 2P 클라이언트가 가짜 투사체를 생성하지 못하도록 오직 1P 서버에서만 사격 승인
		{
			FireProjectile(); // 총알 생성 함수 진입
		}
	}
	else if (LaserMesh)
	{
		LaserMesh->SetVisibility(false); // 표적 상실 시 렌더링 즉각 종료
	}
}

void AFTTurret::FireProjectile()
{
	if (!HasAuthority() || !CurrentTarget || !ProjectileClass) return; // 1P 서버 권한 여부 및 자산 할당 2차 정밀 검증

	bCanAttack = false; // 연사 방어 쿨타임 가동

	FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 투사체 생성 좌표계 할당
	FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, CurrentTarget->GetActorLocation()); // 투사체 발사 각도계 할당

	FTransform SpawnTransform(SpawnRotation, SpawnLocation); // 트랜스폼 데이터 결합
	AFT_ProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AFT_ProjectileBase>(ProjectileClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn); // 메모리 지연 생성 기법 가동

	if (Projectile && AttackDamageEffectClass)
	{
		AFTPlayerCharacterBase* TargetChar = Cast<AFTPlayerCharacterBase>(CurrentTarget);
		if (TargetChar && TargetChar->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle EffectContext = TargetChar->GetAbilitySystemComponent()->MakeEffectContext();
			EffectContext.AddInstigator(this, this); // 대미지 발생 근원지 주체 명시
			FGameplayEffectSpecHandle SpecHandle = TargetChar->GetAbilitySystemComponent()->MakeOutgoingSpec(AttackDamageEffectClass, 1.0f, EffectContext); // 대미지 스펙 핸들러 구성
			Projectile->DamageEffectSpecHandle = SpecHandle; // 투사체 내부 버퍼에 대미지 스펙 최종 주입
		}
	}

	if (Projectile)
	{
		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform); // 실세계 스폰 렌더링 마감
	}

	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, FTimerDelegate::CreateLambda([this]() {
		bCanAttack = true; // 리로드 주기 충족 시 사격 가용 개방
	}), AttackCooldown, false);
}