// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_ChargedShotSkill.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_ChargedShotSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()
public:
	UFT_ChargedShotSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 차징 시간을 계산하기 위한 타임 스탬프
	float ChargeStartTime;
};