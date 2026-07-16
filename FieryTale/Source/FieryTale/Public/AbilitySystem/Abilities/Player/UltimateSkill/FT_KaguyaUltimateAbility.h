// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "FT_KaguyaUltimateAbility.generated.h"

/**
 * 가구야 공주 궁극기: 달의 군대 천인 강림
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaUltimateAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()
    
public:
	UFT_KaguyaUltimateAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnMontageFinished();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
	TSoftObjectPtr<class UAnimMontage> SkillMontage;

	// --- 가구야 궁극기 고유 스펙 ---
	/** 자신 중심 광역 스캔 반경 (기본 1200cm / 12m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
	float AscensionRadius;

	/** 적중 시 시전할 기본 피해량 수치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
	float BaseDamageValue;

	/** 중심부로 적들을 끌고 오는 인장력 배율 승수 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
	float PullForceMultiplier;

	// --- 연동할 GameplayEffect(GE) 클래스 ---
	/** 확정 대미지를 적용할 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

	/** 적들에게 2초간 80% 이동 속도 감산을 주입할 슬로우 디버프 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> SlowDebuffEffectClass;
	
	// 어빌리티 종료 시 태그 제거를 위해 EndAbility를 오버라이드합니다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};