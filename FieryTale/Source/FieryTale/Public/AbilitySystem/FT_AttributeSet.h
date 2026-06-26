// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "FT_AttributeSet.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributeInitialized);

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

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
    /** 현재 체력 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Health)

    /** 최대 체력 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxHealth)

    /** 현재 흡수 보호막 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_Shield)
    FGameplayAttributeData Shield;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, Shield)

    /**  최대 흡수 보호막 한계치  */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MaxShield)
    FGameplayAttributeData MaxShield;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxShield)

    /** TPS 이동 속도 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MoveSpeed)
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MoveSpeed)

    /** 공격력 가중치 계수 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, AttackPower)

    /** 현재 궁극기 게이지 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_UltimateGauge)
    FGameplayAttributeData UltimateGauge;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, UltimateGauge)

    /** 최대 궁극기 요구 게이지 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MaxUltimateGauge)
    FGameplayAttributeData MaxUltimateGauge;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxUltimateGauge)

    /** 현재 무기 탄퍼짐 레벨 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_WeaponSpread)
    FGameplayAttributeData WeaponSpread;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, WeaponSpread)

    /** 최대 무기 탄퍼짐 한계치 */
    UPROPERTY(BlueprintReadOnly, Category = "FT|Attributes", ReplicatedUsing = OnRep_MaxWeaponSpread)
    FGameplayAttributeData MaxWeaponSpread;
    ATTRIBUTE_ACCESSORS(UFT_AttributeSet, MaxWeaponSpread)

protected:
    UFUNCTION() 
    virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
    
    UFUNCTION() 
    virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    
    UFUNCTION() 
    virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);
    
    UFUNCTION() 
    virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield); 
    
    UFUNCTION() 
    virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);
    
    UFUNCTION() 
    virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
    
    UFUNCTION()
    virtual void OnRep_UltimateGauge(const FGameplayAttributeData& OldUltimateGauge);
    
    UFUNCTION()
    virtual void OnRep_MaxUltimateGauge(const FGameplayAttributeData& OldMaxUltimateGauge);
    
    UFUNCTION() 
    virtual void OnRep_WeaponSpread(const FGameplayAttributeData& OldWeaponSpread);
    
    UFUNCTION() 
    virtual void OnRep_MaxWeaponSpread(const FGameplayAttributeData& OldMaxWeaponSpread);
};