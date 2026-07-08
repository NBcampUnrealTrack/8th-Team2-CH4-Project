// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "FTTowerBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGeometryCollectionComponent;
class UFT_AbilitySystemComponent;
class UFT_AttributeSet;
class UDataTable;
class USoundBase;
class UParticleSystem;

UCLASS(Abstract)
class FIERYTALE_API AFTTowerBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTTowerBase(); // 생성자

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override; // 어빌리티 시스템 컴포넌트 주소 반환 인터페이스

protected:
	virtual void BeginPlay() override; // 액터 초기화 기동 루틴

	void OnHealthChanged(const struct FOnAttributeChangeData& Data); // 내구도 속성 변동 콜백 함수

	void StartDestruction(); // 독립 카오스 파쇄 시퀀스 개시 함수

	virtual FGameplayTag GetTeamTag() const { return FGameplayTag(); } // 하위 진영 태그 반환 가상 함수
	virtual FGameplayTag GetStructureTag() const { return FGameplayTag(); } // 하위 구조물 고유 태그 반환 가상 함수
	virtual float GetDefaultMaxHealth() const { return 1000.f; } // 디폴트 내구도 반환 가상 함수
	virtual void NotifyDestroyed() {} // 파생 클래스 특화 통보용 가상 함수

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TurretMesh; // 최상위 루트 외형 담당 스태틱 메시

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox; // 루트 하위에 귀속되는 투사체 피격 박스 콜리전

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> DebrisMesh; // 독립 분쇄 연출을 보장받는 카오스 지오메트리 컬렉션

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent; // GAS ASC 인스턴스

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AttributeSet> AttributeSet; // 체력 스탯 어트리뷰트 세트

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UDataTable> TowerDataTable; // 스탯 데이터 테이블 시트 보관 슬롯

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 데이터 테이블 행 추적 이름

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> DestructionSound; // 파괴 시점 효과음 오디오 소스

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> DestructionEffect; // 파괴 시점 폭발 파티클 소스

	bool bIsDying = false; // 파괴 제어 중복 진입 방어용 플래그
	float MaxDyingTime = 5.f; // 메모리 자동 완전 소거 대기 수명 타임
};