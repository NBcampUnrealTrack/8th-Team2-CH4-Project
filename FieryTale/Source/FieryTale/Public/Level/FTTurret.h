#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "FTTurret.generated.h"

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
	None = 0, // 소속 없음
	BlueTeam = 1, // 블루 팀
	RedTeam = 2 // 레드 팀
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8
{
	None = 0, // 위치 없음
	Left = 1, // 좌측
	Right = 2 // 우측
};

UCLASS()
class FIERYTALE_API AFTTurret : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTTurret(); // 기본 생성자
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override; // ASC 반환
	EFTTurretTeam GetTurretTeam() const { return TurretTeam; } // 포탑 팀 반환
	EFTTurretPosition GetTurretPosition() const { return TurretPosition; } // 포탑 위치 반환

protected:
	virtual void BeginPlay() override; // 게임 시작 시 호출
	virtual void Tick(float DeltaTime) override; // 매 프레임 호출
	void OnHealthChanged(const struct FOnAttributeChangeData& Data); // 체력 변경 이벤트 콜백
	void DebugDestroyTurret(); // 포탑 파괴 로직

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> TurretMesh; // 포탑 메시 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent; // 포탑 ASC

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<class UFT_AttributeSet> AttributeSet; // 포탑 어트리뷰트 셋

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<class UDataTable> TurretDataTable; // 포탑 스탯 데이터테이블

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 포탑 스탯 데이터 행 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretTeam TurretTeam; // 포탑 소속 팀

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretPosition TurretPosition; // 포탑 스폰 위치

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class USoundBase> DestructionSound; // 파괴 사운드

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class UParticleSystem> DestructionEffect; // 파괴 파티클

private:
	FTimerHandle AutoDestroyTimerHandle; // 10초 자동 파괴 타이머 핸들
	FTimerHandle DebugDamageTimerHandle; // 체력바 테스트를 위해 2초마다 대미지 적용 타이머 핸들

	FVector InitialLocation; // 파괴 시 침강을 위한 초기 위치
	bool bIsDying; // 파괴 상태 플래그
	float DyingTimer; // 파괴 연출 경과 시간
	float MaxDyingTime; // 파괴 연출 지속 시간

	void ApplyDebugDamage(); // 테스트용 강제 대미지 로직
};