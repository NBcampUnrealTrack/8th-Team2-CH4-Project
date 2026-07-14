// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "FT_WolfRoarAbility.generated.h"

/**
 * 빨간 망토 궁극기: 굶주린 늑대의 포효
 */
UCLASS()
class FIERYTALE_API UFT_WolfRoarAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()

public:
	UFT_WolfRoarAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnMontageFinished();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
	TObjectPtr<class UAnimMontage> SkillMontage;

	// 어빌리티 종료 시 궁극기 태그를 제거하기 위해 EndAbility를 오버라이드합니다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// --- 늑대 포효 물리 판정 스펙 ---
	/** 사거리 수치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	float HuntRadius;

	/** 부채꼴 유효 각도 (내각 기준, 기본 90도 전방 스캔) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	float ConeAngle;

	/** 기본 피해량 수치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	float BaseDamageValue;

	// --- 연동할 GameplayEffect(GE) 클래스 ---
	/** 대미지 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

	/** 속박 디버프 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> RootGameplayEffectClass;
};