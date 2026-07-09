// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "FTTowerBase.generated.h"

class UFT_AbilitySystemComponent;
class UFT_AttributeSet;

UCLASS(Abstract)
class FIERYTALE_API AFTTowerBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTTowerBase(); // 구조물 기초 속성 할당용 부모 생성자 선언
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override; // GAS 접근용 인터페이스 오버라이드
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // 멀티플레이 변수 복제 등록용 필수 함수

	void OnHealthChanged(const FOnAttributeChangeData& Data); // 자식 클래스의 델리게이트 접근 권한 에러를 해결하기 위해 public으로 개방된 콜백 함수

protected:
	virtual void BeginPlay() override; // 데이터베이스 및 GAS 세팅 초기화 루틴
	
	UFUNCTION()
	virtual void OnRep_IsDestroyed(); // 클라이언트 동기화의 핵심인 상태 변수 복제 알림 함수

	virtual void PerformDestructionEffects() {} // 1P/2P 양쪽 화면에서 실제 시각 연출을 가동할 가상 함수
	virtual void NotifyGameMode() {} // 오직 1P 서버에서만 점수와 승패를 통보할 가상 함수

	virtual FGameplayTag GetTeamTag() const { return FGameplayTag(); } // 하위 클래스 진영 태그 반환 통로
	virtual FGameplayTag GetStructureTag() const { return FGameplayTag(); } // 하위 클래스 객체 태그 반환 통로
	virtual float GetDefaultMaxHealth() const { return 1000.f; } // 하위 클래스 기본 체력 반환 통로

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent; // 데미지 처리 및 태그 관리용 네이티브 ASC

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UFT_AttributeSet> AttributeSet; // 체력 보관용 스탯 세트

	UPROPERTY(ReplicatedUsing = OnRep_IsDestroyed)
	bool bIsDestroyed; // true로 바뀌는 순간 모든 클라이언트 화면에서 붕괴 연출을 동시 발동시키는 네트워크 변수

	bool bIsDying; // 서버에서 파괴 로직이 두 번 겹쳐 실행되는 것을 막는 로컬 락 플래그
};