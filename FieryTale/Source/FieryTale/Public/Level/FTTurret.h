// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTTurret.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGeometryCollectionComponent;
class AFT_ProjectileBase;
class UGameplayEffect;
class UDataTable;
class USoundBase;
class UParticleSystem;

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
	BlueTeam, // 블루 진영 식별자
	RedTeam   // 레드 진영 식별자
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8
{
	NonePosition, // 컴파일러 충돌 회피용 기본값
	Left, // 좌측 라인 식별자
	Right // 우측 라인 식별자
};

UCLASS()
class FIERYTALE_API AFTTurret : public AFTTowerBase
{
	GENERATED_BODY()

public:
	AFTTurret(); // 포탑 생성자

	EFTTurretTeam GetTurretTeam() const { return TurretTeam; } // 포탑 팀 정보 반환
	EFTTurretPosition GetTurretPosition() const { return TurretPosition; } // 포탑 라인 정보 반환

protected:
	virtual void BeginPlay() override; // 포탑 특화 데이터베이스 연동 및 타이머 가동

	virtual FGameplayTag GetTeamTag() const override; // 진영 태그 오버라이드
	virtual FGameplayTag GetStructureTag() const override; // 타입 태그 오버라이드
	virtual float GetDefaultMaxHealth() const override { return 1000.f; } // 기본 포탑 체력 오버라이드

	virtual void PerformDestructionEffects() override; // 1P/2P 메쉬 스왑 및 카오스 재생용 렌더링 오버라이드
	virtual void NotifyGameMode() override; // 1P 스코어 통보용 오버라이드

	void TrackNearestEnemy(); // 조준선 유지 알고리즘
	void FireProjectile(); // 투사체 발사 알고리즘

	UFUNCTION(BlueprintImplementableEvent, Category = "Turret")
	void OnTurretDestroyed(); // 블루프린트 카오스 캐시 재생을 위해 엔진으로 넘겨주는 가상 이벤트

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TurretMesh; // 블루프린트 오작동을 막기 위해 보존된 원본 타워 메쉬

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox; // 투사체 피격 오버랩 전용 박스 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LaserMesh; // 최적화된 독립 조준선 빔 메쉬

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> DebrisMesh; // 붕괴 연출을 위한 카오스 파편 컬렉션

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UDataTable> TurretDataTable; // 데이터 시트 참조 슬롯

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 데이터 시트 열 식별자

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<AFT_ProjectileBase> ProjectileClass; // 발사할 총알 블루프린트

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<UGameplayEffect> AttackDamageEffectClass; // 명중 시 적용할 데미지 스펙

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	EFTTurretTeam TurretTeam; // 편집 가능한 진영 변수

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretPosition TurretPosition; // 편집 가능한 라인 변수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> DestructionSound; // 파괴 시 출력할 폭발음

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> DestructionEffect; // 파괴 시 출력할 흙먼지 이펙트

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float DetectionRange; // 적 탐지 사거리

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float AttackCooldown; // 포탑 연사 간격

	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	TObjectPtr<AActor> CurrentTarget; // 현재 타겟 유지 포인터

	FTimerHandle TargetTrackingTimerHandle; // 조준 추적용 타이머
	FTimerHandle AttackTimerHandle; // 쿨타임 제어용 타이머
	bool bCanAttack; // 사격 가용 플래그
};