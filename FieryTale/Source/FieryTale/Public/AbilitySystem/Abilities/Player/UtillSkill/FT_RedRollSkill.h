// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_RedRollSkill.generated.h"

/**
 * 
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
	// 루트 모션 태스크가 완료되었을 때 호출될 콜백 함수입니다
	UFUNCTION()
	void OnRootMotionTimedOut();

	// 기획 스펙 회피 이동 속도 파워 수치입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float RollSpeed;

	// 회피 기동이 유지될 총 시간 초 단위입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float RollDuration;
};