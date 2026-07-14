// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FT_GameplayAbility.generated.h"

/** 
 * FieryTale 프로젝트의 기본 게임플레이 어빌리티 클래스입니다.
 * 공통 속성 및 유틸리티 함수를 제공합니다.
 */
UCLASS()
class FIERYTALE_API UFT_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
    
public:
    
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    
	/** 
	 * 디버깅용 시각화 활성화 여부
	 * 에디터에서 활성화 시, 스킬의 히트박스 및 사거리를 시각화하는 데 사용됩니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Debug")
	bool bDrawDebugs = false;
    
private:
	UPROPERTY()
	FGameplayTagContainer RuntimeCooldownTags;
protected:

	// GAS 쿨타임 오버라이드
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 디테일 패널에서 지정할 쿨타임 태그입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Cooldown")
	FGameplayTag CooldownTag;
};