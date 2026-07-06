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
	// [보정 완료] 타이머 매니저 및 비동기 몽타주 인터럽트 델리게이트 수신을 위해 UFUNCTION 관문을 완착합니다.
	/** 1초 장전 채널링이 정상 만료되었거나 CC기에 장전이 취소되었을 때 상태 정리를 격발할 콜백 함수 */
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
	
	/** 
	 * 은탄 관통 적중 시 가동될 종합 명세서 GameplayEffect 클래스 
	 * (C++의 50.0f 데미지와 에디터의 2초 지속 50% 슬로우 디버프를 결합하는 통로)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> SilverBulletImpactEffectClass;
    
};