// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "FT_AttributeSet.generated.h"

/**
 * [ATTRIBUTE_ACCESSORS 매크로]
 * 각 어트리뷰트(Health, Mana 등)에 대해 Get, Set, Init 함수를 자동으로 생성해줍니다.
 * 예: GetHealth(), SetHealth(), InitHealth() 등을 코드에서 바로 쓸 수 있게 됩니다.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** 어트리뷰트의 초기화가 완료되었을 때 UI나 시스템에 알리기 위한 델리게이트입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributeInitialized);

/** * [UCC_AttributeSet]
 * 캐릭터의 수치적 상태(체력, 마나, 공격력 등)를 정의하고 관리하는 클래스입니다.*/
UCLASS()
class FIERYTALE_API UFT_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	
	/** * 멀티플레이어 환경에서 어트리뷰트가 복제되는 방식을 설정합니다. */
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	UPROPERTY(BlueprintAssignable)
	FAttributeInitialized OnAttributeInitialized;
	
	UPROPERTY(ReplicatedUsing = OnRep_AttributeInitialized)
	bool bAttributeInitialized = false;
	
	UFUNCTION()
	void OnRep_AttributeInitialized();
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(ThisClass, Health)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MAXHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(ThisClass, MaxHealth)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(ThisClass, Mana)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(ThisClass, MaxMana)
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
};
