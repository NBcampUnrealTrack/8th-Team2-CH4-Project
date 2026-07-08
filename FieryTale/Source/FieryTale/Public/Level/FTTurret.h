// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTTurret.generated.h"

class AFT_ProjectileBase;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
    BlueTeam, // 블루 진영 식별자
    RedTeam   // 레드 진영 식별자
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8
{
    None, // 기본 상태 지정
    Left, // 좌측 라인 배치 식별자
    Right // 우측 라인 배치 식별자
};

UCLASS()
class FIERYTALE_API AFTTurret : public AFTTowerBase
{
    GENERATED_BODY()

public:
    AFTTurret(); // 생성자

    EFTTurretTeam GetTurretTeam() const { return TurretTeam; } // 포탑 팀 정보 반환 함수
    EFTTurretPosition GetTurretPosition() const { return TurretPosition; } // 포탑 배치 상세 라인 반환 함수

protected:
    virtual void BeginPlay() override; // 초기화 루틴

    virtual FGameplayTag GetTeamTag() const override; // 팀 식별 태그 반환 오버라이드
    virtual FGameplayTag GetStructureTag() const override; // 구조물 타입 태그 반환 오버라이드
    virtual float GetDefaultMaxHealth() const override { return 1000.f; } // 기본 최대 체력값 반환 오버라이드
    virtual void NotifyDestroyed() override; // 소멸 상태 확정 알림 오버라이드

    void TrackNearestEnemy(); // 레이저 조준 추적 연산 핵심 루틴 함수
    void FireProjectile(); // 대미지 스펙 동기화 투사체 방출 함수

    UFUNCTION(BlueprintImplementableEvent, Category = "Turret")
    void OnTurretDestroyed(); // 블루프린트 연출 제어용 가상 이벤트 함수

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> LaserMesh; // 독립 조준선 레이저 스태틱 메시 컴포넌트

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
    TSubclassOf<AFT_ProjectileBase> ProjectileClass; // 발사할 대상 투사체 블루프린트 클래스 소스 변수

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
    TSubclassOf<UGameplayEffect> AttackDamageEffectClass; // 투사체 피격 주입용 대미지 이펙트 클래스 변수

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
    EFTTurretTeam TurretTeam; // 편집 가능한 포탑 소속 진영 데이터 변수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
    EFTTurretPosition TurretPosition; // 포탑 상세 배치 라인 변수

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
    float DetectionRange; // 표적 감지 최대 유효 사거리

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
    float AttackCooldown; // 포탑 사격 연사 지연 주 기 시간 쿨타임 변수

	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	TObjectPtr<AActor> CurrentTarget; // 락온 홀딩 타겟 액터 포인터 변수

	FTimerHandle TargetTrackingTimerHandle; // 조준 추적 타이머 핸들
	FTimerHandle AttackTimerHandle; // 사격 리로드 제어 타이머 핸들
	bool bCanAttack; // 발사 가용 타이밍 검증 조건자
};