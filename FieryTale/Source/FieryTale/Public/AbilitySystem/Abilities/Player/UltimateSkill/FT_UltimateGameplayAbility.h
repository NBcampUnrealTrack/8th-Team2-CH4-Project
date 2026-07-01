// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_UltimateGameplayAbility.generated.h"

/**
 * FieryTale 모든 영웅의 궁극기(Q 스킬) 전용 베이스 클래스
 */
UCLASS()
class FIERYTALE_API UFT_UltimateGameplayAbility : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_UltimateGameplayAbility();

	// 능력을 발동할 수 있는지 게이지 자원을 선제 검사합니다
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// 궁극기 격발 성공 시 게이지를 완전히 소모시킵니다
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
};