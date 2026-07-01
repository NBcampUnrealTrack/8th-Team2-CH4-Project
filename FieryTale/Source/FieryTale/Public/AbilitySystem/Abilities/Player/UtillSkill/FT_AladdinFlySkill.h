// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AladdinFlySkill.generated.h"

class AFTPlayerCharacterBase;

/**
 * 알라딘 Shift 기술 - 양탄자 기류 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_AladdinFlySkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_AladdinFlySkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 5초 비행 지속 시간이 만료되었을 때 호출될 타이머 콜백 함수
	UFUNCTION()
	void OnFlyDurationExpired();

	// 5초 유지를 정밀하게 제어할 순정 타이머 핸들
	FTimerHandle FlyDurationTimerHandle;

	// --- 알라딘 비행 고유 스펙 ---
	/** 양탄자 비행이 유지될 총 지속 시간 (기본값 5.0초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
	float FlyDuration;

	/** 비행 중 이동 속도 페널티 배율 (기본값 0.7f -> 30% 감소) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
	float FlySpeedPenaltyMultiplier;

private:
	// ◄◄◄ [완벽 보정] 실시간 디버프 중첩 시 이속 릭을 방지하기 위한 순정 원본 최대 속도 백업 장치
	float OriginalMaxWalkSpeed;
};