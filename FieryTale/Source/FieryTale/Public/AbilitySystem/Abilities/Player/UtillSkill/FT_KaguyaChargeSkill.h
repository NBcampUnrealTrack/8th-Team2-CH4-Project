// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_KaguyaChargeSkill.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaChargeSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_KaguyaChargeSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnChargeFinished();

	// 돌진 속도 및 돌진 유지 시간입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float ChargeSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float ChargeDuration;
};