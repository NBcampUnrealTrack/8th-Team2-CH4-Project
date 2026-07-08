// Copyright Epic Games, Inc. All Rights Reserved.
#include "Level/FTTurret.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"
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
	LaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserMesh")); // 레이저 컴포넌트 생성
	LaserMesh->SetupAttachment(RootComponent); // 최상위 루트인 TurretMesh에 하위 부착 연결
	LaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 레이저 피격 판정 배제
	LaserMesh->SetUsingAbsoluteLocation(true); // 절대 월드 배치 좌표축 고정 적용
	LaserMesh->SetUsingAbsoluteRotation(true); // 절대 월드 회전축 고정 적용
	LaserMesh->SetUsingAbsoluteScale(true); // 절대 축척 배율 고정 적용

	TurretTeam = EFTTurretTeam::BlueTeam; // 기본 진영 초기 블루 배정
	TurretPosition = EFTTurretPosition::None;

	DetectionRange = 1200.0f; // 기본 사거리 범위 초기화
	AttackCooldown = 1.5f; // 사격 주기 초기화
	CurrentTarget = nullptr;
	bCanAttack = true;
}

void AFTTurret::BeginPlay()
{
	Super::BeginPlay(); // 부모 클래스 초기화 루틴 기동

	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		if (UFTHealthWidgetComponent* FTHealthComp = Cast<UFTHealthWidgetComponent>(HealthWidgetComp))
		{
			FTHealthComp->UpdateASCBinding(); // 위젯 화이트아웃 강제 교정 연동
		}
	}

	if (LaserMesh)
	{
		LaserMesh->SetVisibility(false); // 레이저 초기 숨김 가시성 제어
	}

	GetWorld()->GetTimerManager().SetTimer(TargetTrackingTimerHandle, this, &AFTTurret::TrackNearestEnemy, 0.03f, true); // 탐지 추적 타이머 기동 등록
}

FGameplayTag AFTTurret::GetTeamTag() const
{
	return (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 고유 팀 태그 선별 반환
}

FGameplayTag AFTTurret::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Turret; // 포탑 구조물 전용 태그 반환
}

void AFTTurret::TrackNearestEnemy()
{
	if (bIsDying) return; // 부모 파괴 플래그 동작 감지 시 조준 즉각 완전 중단

	if (CurrentTarget && (GetDistanceTo(CurrentTarget) > DetectionRange))
	{
		CurrentTarget = nullptr; // 사거리 이탈 타겟 소거
	}

	if (CurrentTarget)
	{
		AFTPlayerCharacterBase* TargetChar = Cast<AFTPlayerCharacterBase>(CurrentTarget);
		if (TargetChar && TargetChar->GetAbilitySystemComponent())
		{
			if (TargetChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
			{
				CurrentTarget = nullptr; // 사망 표적 소거
			}
		}
	}

	if (!CurrentTarget)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFTPlayerCharacterBase::StaticClass(), FoundActors); // 타겟 검색
        
		AActor* NearestEnemy = nullptr;
		float ClosestDistance = DetectionRange;

		for (AActor* Actor : FoundActors)
		{
			AFTPlayerCharacterBase* EnemyChar = Cast<AFTPlayerCharacterBase>(Actor); // 타입 캐스팅 검증
			if (!EnemyChar) continue; // 실패 시 패스

			UAbilitySystemComponent* TargetASC = EnemyChar->GetAbilitySystemComponent(); // ASC 확인
			if (!TargetASC || TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue; // 사망자 배제

			bool bIsEnemy = false;
			if (TurretTeam == EFTTurretTeam::BlueTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsEnemy = true;
			if (TurretTeam == EFTTurretTeam::RedTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsEnemy = true;

			if (bIsEnemy)
			{
				float Distance = GetDistanceTo(EnemyChar); // 직선 거리 산출
				if (Distance < ClosestDistance)
				{
					ClosestDistance = Distance; // 수치 교체
					NearestEnemy = EnemyChar; // 주소 보관
				}
			}
		}
		CurrentTarget = NearestEnemy; // 조준 타겟 확정
	}

	if (CurrentTarget && LaserMesh)
	{
		LaserMesh->SetVisibility(true); // 조준선 렌더링 활성화

		FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 레이저 출발 원점 좌표 연산
		FVector EndLocation = CurrentTarget->GetActorLocation(); // 대상 기하 좌표 수집
        
		FVector MidLocation = (StartLocation + EndLocation) * 0.5f; // 원점 보정 중간 위치 산출
		float Distance = FVector::Distance(StartLocation, EndLocation); // 직선 거리 스케일 산출
		FRotator LaserRot = UKismetMathLibrary::FindLookAtRotation(StartLocation, EndLocation); // 각도 정밀 산출
        
		LaserMesh->SetWorldLocation(MidLocation); // 위치 갱신
		LaserMesh->SetWorldRotation(LaserRot); // 회전 정렬
		LaserMesh->SetWorldScale3D(FVector(Distance / 100.0f, 0.05f, 0.05f)); // 배율 스케일 팽창

		if (bCanAttack)
		{
			FireProjectile(); // 조건 만족 시 사격 함수 호출
		}
	}
	else if (LaserMesh)
	{
		LaserMesh->SetVisibility(false); // 표적 부재 시 가시성 은닉
	}
}

void AFTTurret::FireProjectile()
{
	if (!CurrentTarget || !ProjectileClass) return; // 자산 안정성 검증 실패 시 호출 즉각 거부

	bCanAttack = false; // 연사 방지를 위해 권한 즉각 폐쇄

	FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 투사체 스폰 연산 위치 설정
	FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, CurrentTarget->GetActorLocation()); // 사격 각도 정렬 수립

	FTransform SpawnTransform(SpawnRotation, SpawnLocation); // 변형 데이터 결합
	AFT_ProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AFT_ProjectileBase>(ProjectileClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn); // 지연 생성 방출

	if (Projectile && AttackDamageEffectClass)
	{
		AFTPlayerCharacterBase* TargetChar = Cast<AFTPlayerCharacterBase>(CurrentTarget);
		if (TargetChar && TargetChar->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle EffectContext = TargetChar->GetAbilitySystemComponent()->MakeEffectContext();
			EffectContext.AddInstigator(this, this); // 대미지 가해자 주체 배정
			FGameplayEffectSpecHandle SpecHandle = TargetChar->GetAbilitySystemComponent()->MakeOutgoingSpec(AttackDamageEffectClass, 1.0f, EffectContext); // 대미지 가해 명세서 연산
			Projectile->DamageEffectSpecHandle = SpecHandle; // 생성된 투사체 버퍼 스펙 주입
		}
	}

	if (Projectile)
	{
		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform); // 스폰 최종 완결 처리
	}

	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, FTimerDelegate::CreateLambda([this]() {
		bCanAttack = true; // 쿨타임 주기 만료 시 사격 권한 재개방
	}), AttackCooldown, false);
}

void AFTTurret::NotifyDestroyed()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 구조물 가해 비활성 상태 주입
	}

	GetWorld()->GetTimerManager().ClearTimer(TargetTrackingTimerHandle); // 조준 루프 타이머 소거
	GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle); // 사격 리로드 타이머 소거

	if (LaserMesh)
	{
		LaserMesh->SetVisibility(false); // 조준선 시각 차단 종료
	}

	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		HealthWidgetComp->SetVisibility(false); // 포탑 체력바 UI 레이아웃 소거
	}

	OnTurretDestroyed(); // 블루프린트 단 연출 신호 전달

	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->TurretDestroyed(this); // 매치 스코어 규칙 처리 핸들러 호출
		}
	}
}