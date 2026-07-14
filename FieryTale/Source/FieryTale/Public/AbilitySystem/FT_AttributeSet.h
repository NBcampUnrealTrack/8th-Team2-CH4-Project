// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "FT_AttributeSet.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributeInitialized);

// GAS 속성 집합의 Getter, Setter, Init 함수를 생성하는 매크로입니다.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * FieryTale 게임 내 모든 액터(영웅, 미니언, 포탑)가 공유하는 기본 속성 집합입니다.
 * 체력, 보호막, 이동 속도, 궁극기 자원 및 무기 탄퍼짐 등의 프로퍼티를 관리합니다.
 */
UCLASS()
class FIERYTALE_API UFT_AttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UFT_AttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
    /** 현재 체력 수치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Health)

    /** 최대 체력 한계치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxHealth)

    /** 현재 흡수 보호막 수치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_Shield)
    FGameplayAttributeData Shield;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Shield)

    /** 최대 흡수 보호막 한계치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MaxShield)
    FGameplayAttributeData MaxShield;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxShield)

    /** 현재 TPS 실시간 이동 속도 수치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MoveSpeed)
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MoveSpeed)

    /** 최대 TPS 이동 속도 보존 수치 (디버프 복구 및 연산용 기본 속성) */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MaxMoveSpeed)
    FGameplayAttributeData MaxMoveSpeed;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxMoveSpeed)

    /** 공격력 가중치 계수 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, AttackPower)

    /** 현재 궁극기 자원 게이지 수치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_UltimateGauge)
    FGameplayAttributeData UltimateGauge;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, UltimateGauge)

    /** 궁극기 활성화를 위한 최대 요구 게이지 수치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MaxUltimateGauge)
    FGameplayAttributeData MaxUltimateGauge;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxUltimateGauge)

    /** 현재 무기 탄퍼짐 변동 레벨 (빨간 망토 전용 보정 기획 연동) */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_WeaponSpread)
    FGameplayAttributeData WeaponSpread;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, WeaponSpread)

    /** 무기 탄퍼짐 최대 확장 한계치 */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes", ReplicatedUsing = OnRep_MaxWeaponSpread)
    FGameplayAttributeData MaxWeaponSpread;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxWeaponSpread)

    /** 
     * 대미지 연산용 메타 속성
     * GEEC_Damage의 계산 수치를 받아 PostGameplayEffectExecute에서 체력 및 보호막 차감을 처리합니다.
     * 서버-클라이언트 간 동기화(Replication)가 불필요한 일회성 속성입니다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Attributes")
    FGameplayAttributeData Damage;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Damage)

protected:
    // 네트워크 복제 상태 추적을 위한 델리게이트 함수입니다.
    
    UFUNCTION() 
    virtual void OnRep_Health(const FGameplayAttributeData OldHealth);
    
    UFUNCTION() 
    virtual void OnRep_MaxHealth(const FGameplayAttributeData OldMaxHealth);
    
    UFUNCTION() 
    virtual void OnRep_Shield(const FGameplayAttributeData OldShield);
    
    UFUNCTION() 
    virtual void OnRep_MaxShield(const FGameplayAttributeData OldMaxShield); 
    
    UFUNCTION() 
    virtual void OnRep_MoveSpeed(const FGameplayAttributeData OldMoveSpeed);

    UFUNCTION() 
    virtual void OnRep_MaxMoveSpeed(const FGameplayAttributeData OldMaxMoveSpeed);
    
    UFUNCTION() 
    virtual void OnRep_AttackPower(const FGameplayAttributeData OldAttackPower);
    
    UFUNCTION()
    virtual void OnRep_UltimateGauge(const FGameplayAttributeData OldUltimateGauge);
    
    UFUNCTION()
    virtual void OnRep_MaxUltimateGauge(const FGameplayAttributeData OldMaxUltimateGauge);
    
    UFUNCTION() 
    virtual void OnRep_WeaponSpread(const FGameplayAttributeData OldWeaponSpread);
    
    UFUNCTION() 
    virtual void OnRep_MaxWeaponSpread(const FGameplayAttributeData OldMaxWeaponSpread);
};