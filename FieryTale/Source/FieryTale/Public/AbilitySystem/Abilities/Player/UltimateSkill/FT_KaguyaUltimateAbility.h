// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FT_UltimateGameplayAbility.h"
#include "FT_KaguyaUltimateAbility.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaUltimateAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFT_KaguyaUltimateAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// [기획 스펙] 자신 중심 반경 1200cm (12m)
	UPROPERTY(EditDefaultsOnly, Category = "Kaguya | Combat")
	float AscensionRadius = 1200.f;

	// [기획 스펙] 확정 대미지 120
	UPROPERTY(EditDefaultsOnly, Category = "Kaguya | Combat")
	float BaseDamageValue = 120.f;

	// [기획 스펙] 중심부로 당겨지는 힘의 세기 보정값
	UPROPERTY(EditDefaultsOnly, Category = "Kaguya | Combat")
	float PullForceMultiplier = 1.5f;

	// [에셋 매핑] 대미지 및 80% 슬로우 디버프 에셋 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Kaguya | Effects")
	TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Kaguya | Effects")
	TSubclassOf<class UGameplayEffect> SlowDebuffEffectClass;
};
