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
	// 1초 장전 메커니즘을 제어할 타이머 핸들
	FTimerHandle ChannellingTimerHandle;

	// 1초 장전이 끝난 후 실제로 탄환을 발사하는 함수
	void FireSilverBullet();

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Combat")
	float BaseDamage;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Combat")
	float ChannellingDuration;
	
};