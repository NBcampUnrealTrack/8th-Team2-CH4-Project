#include "Level/FTTurret.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/FTTags.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Character/FTCharacterBase.h"
#include "Object/FT_ProjectileBase.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "Core/Game/FTGameMode.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

AFTTurret::AFTTurret()
{
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh")); // 포탑 외형 생성
	SetRootComponent(TurretMesh); // 루트 지정
	TurretMesh->SetCollisionProfileName(TEXT("BlockAllGeneric")); // 물리 차단 지정

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox")); // 판정 박스 생성
	CollisionBox->SetupAttachment(TurretMesh); // 타워 외형 부착
	CollisionBox->SetBoxExtent(FVector(100.f, 100.f, 200.f)); // 부피 셋업
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 오버랩 활성화
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic); // 판정 채널 셋업
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap); // 겹침 감지 셋업
	CollisionBox->SetGenerateOverlapEvents(true); // 이벤트 개방

	LaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserMesh")); // 빔 연출 생성
	LaserMesh->SetupAttachment(TurretMesh); // 부착
	LaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 물리 차단
	LaserMesh->SetUsingAbsoluteLocation(true); // 좌표계 독립
	LaserMesh->SetUsingAbsoluteRotation(true); // 회전계 독립
	LaserMesh->SetUsingAbsoluteScale(true); // 비율계 독립

	DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh")); // 파편 객체 생성
	DebrisMesh->SetupAttachment(TurretMesh); // 부착

	TurretTeam = EFTTurretTeam::BlueTeam; // 진영 기본값
	TurretPosition = EFTTurretPosition::NonePosition; // 라인 기본값

	DetectionRange = 1200.0f; // 사거리 초기화
	AttackCooldown = 1.5f; // 연사 쿨타임 초기화
	HomingAcceleration = 50000.0f; // 기본 유도 가속도 세팅
	DestructionImpulseRadius = 500.0f; // 자체 폭발 반경 초기화
	DestructionImpulseStrength = 10000.0f; // 에디터 개방용 붕괴 충격량 초기화
	CurrentTarget = nullptr; // 타겟 초기화
	bCanAttack = true; // 사격 가용 초기화
	TurretDummyInstigator = nullptr; // 가상 폰 초기화
}

void AFTTurret::BeginPlay()
{
	float TargetMaxHealth = 1000.0f; // 초기 체력 설정
	
	if (TurretDataTable && !DataRowName.IsNone())
	{
		if (FFTStructureData* RowData = TurretDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
		{
			TargetMaxHealth = RowData->MaxHealth; // 시트 체력 덮어쓰기
		}
	}

	if (AttributeSet && AbilitySystemComponent)
	{
		AttributeSet->InitMaxHealth(TargetMaxHealth); // 최대 체력 세팅
		AttributeSet->InitHealth(TargetMaxHealth); // 현재 체력 세팅
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged); // 체력 리스너 부착
	}

	Super::BeginPlay(); // 기초 연동

	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		if (UFTHealthWidgetComponent* FTHealthComp = Cast<UFTHealthWidgetComponent>(HealthWidgetComp))
		{
			FTHealthComp->UpdateASCBinding(); // UI 수동 바인딩
		}
	}

	if (LaserMesh) LaserMesh->SetVisibility(false); // 조준선 은닉

	GetWorld()->GetTimerManager().SetTimer(TargetTrackingTimerHandle, this, &AFTTurret::TrackNearestEnemy, 0.03f, true); // 탐지 루프 가동
}

FGameplayTag AFTTurret::GetTeamTag() const
{
	return (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 진영 태그 반환
}

FGameplayTag AFTTurret::GetStructureTag() const
{
	return FTTags::FTObjectType::Structure_Turret; // 포탑 태그 반환
}

void AFTTurret::PerformDestructionEffects()
{
	if (AbilitySystemComponent) AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted); // 무력화 태그 삽입

	GetWorld()->GetTimerManager().ClearTimer(TargetTrackingTimerHandle); // 감시 소거
	GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle); // 연사 소거

	if (LaserMesh) LaserMesh->SetVisibility(false); // 조준선 삭제
	if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>()) HealthWidgetComp->SetVisibility(false); // 체력바 삭제

	if (TurretMesh)
	{
		TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 외형 충돌 소거
		TurretMesh->SetVisibility(false); // 기둥 은닉
	}

	if (CollisionBox) CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 피격 박스 정지

	if (DebrisMesh)
	{
		DebrisMesh->SetVisibility(true); // 파편 활성화
		DebrisMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // 트랜스폼 독립으로 서버 물리 동기화 방해 차단
		DebrisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 파편 물리 강제 개방
		DebrisMesh->SetCollisionProfileName(TEXT("Destructible")); // 디스트럭터블 반응 프로필 주입
		DebrisMesh->SetSimulatePhysics(true); // 물리 스레드 가동
		DebrisMesh->AddRadialImpulse(GetActorLocation(), DestructionImpulseRadius, DestructionImpulseStrength, ERadialImpulseFalloff::RIF_Linear, true); // 에디터에서 제어 가능한 변수로 자체 붕괴 충격량 강제 인가
	}

	if (DestructionSound) UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation()); // 사운드 재생
	if (DestructionEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation()); // 이펙트 재생

	OnTurretDestroyed(); // 블루프린트 연출 트리거
}

void AFTTurret::NotifyGameMode()
{
	if (UWorld* World = GetWorld())
	{
		if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
		{
			GameMode->TurretDestroyed(this); // 게임 룰 통보
		}
	}
	SetLifeSpan(5.0f); // 타워 소거
	if (TurretDummyInstigator) TurretDummyInstigator->SetLifeSpan(10.0f); // 가상 폰 동반 소거
}

void AFTTurret::TrackNearestEnemy()
{
	if (bIsDying) return; // 락다운

	if (CurrentTarget && (GetDistanceTo(CurrentTarget) > DetectionRange))
	{
		CurrentTarget = nullptr; // 사거리 이탈 포인터 해제
	}

	if (CurrentTarget)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentTarget); // 타겟 시스템 캐싱
		if (TargetASC && TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
		{
			CurrentTarget = nullptr; // 사망한 타겟 소거
		}
	}

	if (!CurrentTarget)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFTCharacterBase::StaticClass(), FoundActors); // 전역 색인
		
		AActor* NearestEnemy = nullptr; // 거리 갱신 변수
		float ClosestDistance = DetectionRange; // 거리 한계선 세팅

		for (AActor* Actor : FoundActors)
		{
			AFTCharacterBase* EnemyChar = Cast<AFTCharacterBase>(Actor); // 적정 캐릭터 판독
			if (!EnemyChar) continue; // 이탈

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EnemyChar); // 어빌리티 추출
			if (!TargetASC || TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue; // 사망자 배제

			bool bIsEnemy = false; // 진영 비트
			if (TurretTeam == EFTTurretTeam::BlueTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsEnemy = true; // 블루팀 타워 타겟 판별
			if (TurretTeam == EFTTurretTeam::RedTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsEnemy = true; // 레드팀 타워 타겟 판별

			if (bIsEnemy)
			{
				float Distance = GetDistanceTo(EnemyChar); // 타겟 거리 산출
				if (Distance < ClosestDistance)
				{
					ClosestDistance = Distance; // 거리 갱신
					NearestEnemy = EnemyChar; // 새 표적 인가
				}
			}
		}
		CurrentTarget = NearestEnemy; // 최종 표적 확정
	}

	if (CurrentTarget && LaserMesh)
	{
		LaserMesh->SetVisibility(true); // 조준 활성화

		FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 빔 원점 연산
		FVector EndLocation = CurrentTarget->GetActorLocation(); // 빔 끝점 연산
		
		FVector MidLocation = (StartLocation + EndLocation) * 0.5f; // 중심 연산
		float Distance = FVector::Distance(StartLocation, EndLocation); // 길이 추출
		FRotator LaserRot = UKismetMathLibrary::FindLookAtRotation(StartLocation, EndLocation); // 각도 추출
		
		LaserMesh->SetWorldLocation(MidLocation); // 중앙값 동기화
		LaserMesh->SetWorldRotation(LaserRot); // 회전 정렬
		LaserMesh->SetWorldScale3D(FVector(Distance / 100.0f, 0.05f, 0.05f)); // 배율 증축

		if (bCanAttack && HasAuthority())
		{
			FireProjectile(); // 총알 생성 진입
		}
	}
	else if (LaserMesh)
	{
		LaserMesh->SetVisibility(false); // 조준 정지
	}
}

void AFTTurret::FireProjectile()
{
	if (!HasAuthority() || !CurrentTarget || !ProjectileClass) return; // 필수 검증

	bCanAttack = false; // 연사 락다운 적용

	if (!TurretDummyInstigator)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;
		TurretDummyInstigator = GetWorld()->SpawnActor<APawn>(APawn::StaticClass(), GetActorLocation(), GetActorRotation(), SpawnParams); // 가상 대리인 생성
		if (TurretDummyInstigator)
		{
			TurretDummyInstigator->SetActorHiddenInGame(true); // 렌더링 은닉
			TurretDummyInstigator->SetActorEnableCollision(false); // 물리 간섭 차단
			TurretDummyInstigator->SetCanBeDamaged(false); // 무적 처리
			
			UFT_AbilitySystemComponent* DummyASC = NewObject<UFT_AbilitySystemComponent>(TurretDummyInstigator, TEXT("DummyASC")); // 가짜 ASC 부착
			if (DummyASC)
			{
				DummyASC->RegisterComponent(); // 가짜 시스템 기동
				DummyASC->AddLooseGameplayTag(GetTeamTag()); // 진영 위장
			}
		}
	}

	FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f); // 투사체 원점
	FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, CurrentTarget->GetActorLocation()); // 투사체 각도
	FTransform SpawnTransform(SpawnRotation, SpawnLocation); // 트랜스폼 결합

	AFT_ProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AFT_ProjectileBase>(ProjectileClass, SpawnTransform, this, TurretDummyInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn); // 가상 대리인을 발사자로 지정하여 생성

	if (Projectile)
	{
		if (UProjectileMovementComponent* MovementComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			MovementComp->bIsHomingProjectile = true; // 유도 센서 개방
			MovementComp->HomingAccelerationMagnitude = HomingAcceleration; // 유도 가속도 주입
			if (CurrentTarget && CurrentTarget->GetRootComponent())
			{
				MovementComp->HomingTargetComponent = CurrentTarget->GetRootComponent(); // 표적 강제 락온
			}
		}

		if (AttackDamageEffectClass && AbilitySystemComponent)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext(); // 컨텍스트 획득
			EffectContext.AddInstigator(TurretDummyInstigator, this); // 대미지 소유권 위임
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(AttackDamageEffectClass, 1.0f, EffectContext); // 대미지 스펙 생성
			Projectile->DamageEffectSpecHandle = SpecHandle; // 투사체 스펙 주입
		}

		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform); // 실세계 스폰 개시

		if (HasAuthority())
		{
			Multicast_SyncProjectileHoming(Projectile, CurrentTarget); // 유도 렌더링 동기화 호출
		}
	}

	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, FTimerDelegate::CreateLambda([this]() {
		bCanAttack = true; // 사격 락다운 해제
	}), AttackCooldown, false);
}

void AFTTurret::Multicast_SyncProjectileHoming_Implementation(AFT_ProjectileBase* Projectile, AActor* Target)
{
	if (Projectile && Target)
	{
		if (UProjectileMovementComponent* MovementComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			MovementComp->bIsHomingProjectile = true; // 클라이언트 화면 유도 센서 개방
			MovementComp->HomingAccelerationMagnitude = HomingAcceleration; // 클라이언트 화면 유도 가속도 주입
			MovementComp->HomingTargetComponent = Target->GetRootComponent(); // 클라이언트 화면 강제 락온
		}
	}
}