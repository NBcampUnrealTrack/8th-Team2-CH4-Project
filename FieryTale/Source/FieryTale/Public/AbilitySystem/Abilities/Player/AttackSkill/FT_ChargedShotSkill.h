// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_ChargedShotSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * 알라딘 RMB 보조 공격 - 지니의 압착 어빌리티 시스템
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
	// 원형 범위를 지정하거나 격발 시점 차징 연산을 처리하기 위한 타임스탬프
	float ChargeStartTime;

	// 기획 스펙: 지니의 주먹 적중 고정 피해량 (50.0f)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Combat")
	float BaseDamage;

	// 중심부에서 외곽으로 적들을 밀어내기 위한 강도 계수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Combat")
	float KnockbackForce;

	// 대미지 주입 및 가감을 처리할 GameplayEffect 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};