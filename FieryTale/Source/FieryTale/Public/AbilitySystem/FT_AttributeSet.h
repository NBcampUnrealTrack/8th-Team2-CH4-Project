// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributeInitialized);

#include "FT_AttributeSet.generated.h"

// GAS 속성 전용 매크로 정의 (Getter, Setter, Init 함수 자동 생성)
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)


/**
 * FieryTale 게임 내 모든 액터(영웅, 미니언, 포탑)가 공유하는 마스터 속성 집합입니다.
 */
UCLASS()
class FIERYTALE_API UFT_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UFT_AttributeSet();

	// 네트워크 복제 및 값 변경 전후 처리를 위한 오버라이드
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
	/** 현재 체력 (기획 기준 초기 180.f 고정) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Health)

	/** 최대 체력 (기웅 기준 180.f 고정, 버프/증강 등으로 변동 가능) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxHealth)

	/** 흡수 보호막 (알라딘 +40, 가구야 +300, 빨간 망토 +200 등) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Shield)

	/** TPS 이동 속도 (기본값 600 cm/s 상정, 정조준/비행 시 -30% 등 연동) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MoveSpeed)

	/** 공격력 가중치 계수 (기본 1.0f, 앨리스 사형선고 시 0.7f 등 조절용) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, AttackPower)

	/** 궁극기 게이지 (0.0f ~ 100.0f 범위 소유) */
	UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_UltimateGauge)
	FGameplayAttributeData UltimateGauge;
	ATTRIBUTE_ACCESSORS(UFT_AttributeSet, UltimateGauge)

protected:
	// 멀티플레이 데디케이트 서버 복제용 OnRep 함수군
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	UFUNCTION() 
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);
	UFUNCTION() 
	virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);
	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
	UFUNCTION()
	virtual void OnRep_UltimateGauge(const FGameplayAttributeData& OldUltimateGauge);
};