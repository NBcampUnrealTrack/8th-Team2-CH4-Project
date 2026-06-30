// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FT_UltimateGameplayAbility.h"
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

protected:
	/** 늑대 포효 부채꼴 중심 사거리 수치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	float HuntRadius = 800.f;

	/** 부채꼴 유효 각도 (내각 기준, 기본 90도 전방 스캔) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	float ConeAngle = 90.f;

	/** 피격자들에게 주입할 고정 대미지 이펙트 (GEEC_Damage 연동용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	/** 2초간 속박 상태 이상을 부여할 디버프 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Wolf Spec")
	TSubclassOf<UGameplayEffect> RootGameplayEffectClass;
};