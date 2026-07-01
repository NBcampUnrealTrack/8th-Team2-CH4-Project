// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_RedRollSkill.generated.h"

/**
 * 빨간 망토 시프트 생존기 - 루트 모션 기반 회피 구르기 기술 헤더입니다.
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
	/** 순정 부모 쿨다운 가상 함수를 안전하게 개통하여 에디터 락인 GE가 격발되도록 링크합니다. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	/** 루트 모션 가속 타임아웃 만료 시 정리를 전담할 콜백 함수 */
	UFUNCTION()
	void OnRootMotionTimedOut();

protected:
	/** 기획 데이터 락인: 구르기 시 순간 돌파할 루트 모션 이동 속도 수치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Roll Spec")
	float RollSpeed;

	/** 기획 데이터 락인: 구르기 물리 가속이 유지될 지속 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Roll Spec")
	float RollDuration;
};