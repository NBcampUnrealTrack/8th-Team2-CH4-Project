// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_CounterShieldSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * 가구야 공주 RMB 강공격 - 봉래의 탄환 방벽 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_CounterShieldSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_CounterShieldSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 4초 최대 유지 스펙 제어용 지속 타이머 핸들
	FTimerHandle BulwarkDurationTimerHandle;

	// 기획 스펙: 방벽 전개 중 가구야의 이동 속도 감산치 (40% 감소 -> 원래 속도의 0.6배)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Bulwark")
	float MovementPenaltyMultiplier;

	// 기획 스펙: 방벽 최대 유지 시간 (4.0초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Bulwark")
	float MaxDuration;
};