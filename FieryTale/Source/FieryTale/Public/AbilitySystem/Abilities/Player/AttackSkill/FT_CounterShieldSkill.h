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
	// [보정 완료] 비동기 몽타주 태스크 종료 시 표준 어빌리티 종료 관문으로 안전하게 이관할 링커 콜백 함수
	/** 가구야 공주의 방벽 애니메이션 완료 시 안전 이관을 통제할 내부 콜백 함수 */
	UFUNCTION()
	void OnGuardMontageFinished();

protected:
	/** 4초 가드 타임아웃을 통제할 내부 지속 타이머 핸들 */
	FTimerHandle BulwarkDurationTimerHandle;

	// --- 가구야 방벽 고유 스펙 ---
	/** 기획 스펙: 방벽 전개 중 가구야의 이동 속도 감산치 배율 (기본값 0.6f / 원래 속도의 60%로 조정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
	float MovementPenaltyMultiplier;

	/** 기획 스펙: 방벽 최대 유지 시간 (기본값 4.0초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
	float MaxDuration;

private:
	//  .cpp 내부 역연산 왜곡으로 인한 영구 속도 패널티 릭을 차단하기 위한 백업 보관소
	/** 방벽 감산 페널티로 인한 무브먼트 왜곡을 영구 방쇄하기 위한 순정 최대 속도 백업 보관소 */
	float OriginalMaxWalkSpeed;
};