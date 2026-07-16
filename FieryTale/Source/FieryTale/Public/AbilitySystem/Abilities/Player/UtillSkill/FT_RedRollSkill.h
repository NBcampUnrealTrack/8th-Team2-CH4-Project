// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_RedRollSkill.generated.h"

/**
 * 빨간 망토 시프트 생존기 - 루트 모션 기반 회피 구르기 기술입니다.
 */
UCLASS()
class FIERYTALE_API UFT_RedRollSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_RedRollSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 쿨다운 이펙트를 가져옵니다. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	/** 루트 모션 종료 시 호출되는 콜백입니다. */
	UFUNCTION()
	void OnRootMotionTimedOut();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
	TSoftObjectPtr<class UAnimMontage> SkillMontage;

	/** 구르기 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Roll Spec")
	float RollSpeed;

	/** 구르기 지속 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Roll Spec")
	float RollDuration;
};