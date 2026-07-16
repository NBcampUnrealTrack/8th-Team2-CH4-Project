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

	// --- 공통 이동 속도 제어 시스템 ---
	/** 스킬 시전 중 적용할 공통 이동 속도 페널티 이펙트 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Movement")
	TSubclassOf<UGameplayEffect> MovementPenaltyGameplayEffectClass;

	/** 적용된 이동 속도 감소 이펙트의 핸들을 캐싱합니다. */
	FActiveGameplayEffectHandle MovementPenaltyActiveHandle;

	/**
	 * 이동 속도 감소 이펙트를 적용합니다.
	 * @param SpeedMultiplier 디폴트는 0.0f (이동 불가)
	 */
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Movement")
	void ApplyMovementPenalty(float SpeedMultiplier = 0.0f);

	/** 적용된 이동 속도 페널티 이펙트를 해제합니다. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Movement")
	void RemoveMovementPenalty();

	// --- 공용 연출 (Cue) ---
	/** 스킬 시전(버튼 입력) 시 재생할 게임플레이 큐 태그입니다. 비워두면 재생하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
	FGameplayTag SkillCastCueTag;
};