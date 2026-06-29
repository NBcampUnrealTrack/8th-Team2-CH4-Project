// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceStealthDashSkill.generated.h"

/**
 * FieryTale 앨리스 전용 Shift 은신 대시 유틸 스킬 클래스입니다
 */
UCLASS()
class FIERYTALE_API UFT_AliceStealthDashSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_AliceStealthDashSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnDashFinished();

	// 은신 대시 속도 및 거리 제어값입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float DashSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float DashDuration;
};
