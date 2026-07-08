// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "FTTowerBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UFT_AbilitySystemComponent;
class UFT_AttributeSet;
class UDataTable;
class USoundBase;
class UParticleSystem;

/**
 *	터렛/넥서스 등 파괴 가능한 구조물의 공통 베이스.
 *	- 루트 = 박스 콜리전(투사체/스킬이 오버랩으로 피격), 외형 스태틱 메시는 루트에 부착.
 *	- 공통: ASC/AttributeSet, 진영·식별 태그 부여, 데이터테이블 기반 체력 초기화, 피격 사망 연출.
 *	- 서브클래스는 진영 태그 / 식별 태그 / 기본 체력 / 파괴 통보만 제공한다.
 */
UCLASS(Abstract)
class FIERYTALE_API AFTTowerBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTTowerBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	//	체력 0 도달 감지 → 파괴 연출 시작
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);

	//	공통 파괴 연출(사운드/이펙트/침강) 시작 + 서브클래스 파괴 통보
	void StartDestruction();

	// --- 서브클래스가 제공하는 차이점 ---
	//	진영 태그(Team_Blue/Team_Red). 무효 태그를 반환하면 부여하지 않는다.
	virtual FGameplayTag GetTeamTag() const { return FGameplayTag(); }
	//	구조물 식별 태그(Structure_Turret/Structure_Nexus).
	virtual FGameplayTag GetStructureTag() const { return FGameplayTag(); }
	//	데이터테이블 미지정 시 사용할 기본 최대 체력.
	virtual float GetDefaultMaxHealth() const { return 1000.f; }
	//	파괴 시 GameMode 등에 통보(서브클래스별 처리).
	virtual void NotifyDestroyed() {}

	// --- 공통 컴포넌트 ---
	//	루트: 피격 판정용 박스 콜리전. 투사체(OverlapAllDynamic)가 오버랩하도록 동적 타입으로 설정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	//	외형(+선택적 이동 차단). 루트 박스에 부착된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TowerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AttributeSet> AttributeSet;

	// --- 공통 데이터 ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UDataTable> TowerDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> DestructionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> DestructionEffect;

	// --- 파괴 연출 상태 ---
	FVector InitialLocation = FVector::ZeroVector;
	bool bIsDying = false;
	float DyingTimer = 0.f;
	float MaxDyingTime = 2.f;
};
