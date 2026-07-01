// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "FT_SilverBulletSkill.generated.h"

/**
 * RMB 보조 공격 - 은빛 탄환 (1초 장전 후 강력한 관통 사격)
 */
UCLASS()
class FIERYTALE_API UFT_SilverBulletSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_SilverBulletSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// [보정 완료] 타이머 매니저의 정상적인 시퀀스 호출을 위해 리플렉션 UFUNCTION을 완착합니다.
	/** 1초 장전 채널링이 정상 만료되었을 때 탄환 사출 시퀀스를 격발할 콜백 함수 */
	UFUNCTION()
	void FireSilverBullet();

protected:
	/** 1초 장전 메커니즘 안전 제어용 순정 타이머 핸들 */
	FTimerHandle ChannellingTimerHandle;

	// --- 빨간 망토 은탄 고유 스펙 ---
	/** 관통 은탄 적중 시 사출할 기본 피해량 (기본값 50.0f) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|RedRidingHood Spec")
	float BaseDamage;

	/** 은탄 발사 전 유지해야 하는 선행 장전 채널링 시간 (기본값 1.0초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|RedRidingHood Spec")
	float ChannellingDuration;
    
};