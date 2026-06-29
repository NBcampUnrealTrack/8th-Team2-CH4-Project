// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AladdinFlySkill.generated.h"

/**
 * FieryTale 알라딘 전용 Shift 양탄자 비행 유틸 스킬 클래스입니다
 */
UCLASS()
class FIERYTALE_API UFT_AladdinFlySkill : public UFT_GameplayAbility
{GENERATED_BODY()

public:
	UFT_AladdinFlySkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 비행 지속 시간이 만료되었을 때 호출될 타이머 콜백 함수입니다
	void OnFlyDurationExpired();

	// 양탄자 비행이 유지될 총 지속 시간 초 단위입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float FlyDuration;

	// 비행 도중 추가적으로 가산될 이동 속도 보정치입니다
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Movement")
	float FlySpeedBoost;

	FTimerHandle FlyTimerHandle;
    
	// 복구용 원본 걷기 속도 저장 변수입니다
	float OrigWalkSpeed;
};
