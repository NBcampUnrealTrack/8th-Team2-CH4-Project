#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h" 
#include "AbilitySystemInterface.h"
#include "FTTurret.generated.h"

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8		// 블루 레드 진영 구분 열거형 클래스
{
	None = 0,		// 초기 설정 누락 or 중립 상태
	BlueTeam = 1,	// 블루 진영 소속
	RedTeam = 2		// 레드 진영 소속
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8	// 맵 배치 좌우 구분 열거형 클래스
{
	None = 0,		// 위치 설정 누락
	Left = 1,		// 좌측 라인
	Right = 2		// 우측 라인
};

UCLASS()
class FIERYTALE_API AFTTurret : public AActor, public IAbilitySystemInterface // 전장 포탑 오브젝트 클래스 생성
{
	GENERATED_BODY()
	
public:	
	AFTTurret();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	EFTTurretTeam GetTurretTeam() const { return TurretTeam; }

	EFTTurretPosition GetTurretPosition() const { return TurretPosition; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void DebugDestroyTurret();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") 
	TObjectPtr<class UStaticMeshComponent> TurretMesh; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS") 
	TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent; // GAS 능력을 발동하고 태그를 수용

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<class UFT_AttributeSet> AttributeSet; // 체력 및 방어력 등 스탯 정보 실체를 물리적으로 저장하

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") 
	TObjectPtr<class UDataTable> TurretDataTable; // 포탑용 능력치 정보가 든 외부 데이터 테이블 연결 참조 변수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 데이터 테이블 파일 안에서 이 특정 포탑이 추적하여 파싱해야 할 고유 행 문자열 키 변수

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretTeam TurretTeam; // 배치 시 어떤 팀 영역에 세워졌는지 판정하는 소속 진영 제어 변수

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretPosition TurretPosition; // 배치 시 어떤 라인에 세워졌는지 판정하는 공간 방향 제어 변수
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class USoundBase> DestructionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class UParticleSystem> DestructionEffect;

private:
	FTimerHandle AutoDestroyTimerHandle;
	FVector InitialLocation;
	bool bIsDying;
	float DyingTimer;
	float MaxDyingTime;
};