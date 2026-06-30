// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceStealthDashSkill.generated.h"

class AFTPlayerCharacterBase;

/**
 * 앨리스 Shift 기술 - 토끼의 약 어빌리티 시스템
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
	// 2초 버프 지속 시간이 끝났을 때 원래 크기와 속도로 되돌릴 콜백 함수
	UFUNCTION()
	void OnShrinkBuffFinished();

	// 2초 유지를 칼같이 제어할 버프 타이머 핸들
	FTimerHandle ShrinkBuffDurationTimerHandle;

	// 기획 스펙: 몸이 작아진 동안 적용될 이동 속도 배율 (50% 증가 -> 1.5)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|ShrinkBuff")
	float MovementSpeedMultiplier;

	// 기획 스펙: 버프 총 유지 시간 (2.0초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|ShrinkBuff")
	float MaxDuration;

	// 히트박스 스케일 감소 처리를 위한 목표 스케일 값 (예: 원래 크기의 0.5배)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|ShrinkBuff")
	float TargetMeshScale;
};