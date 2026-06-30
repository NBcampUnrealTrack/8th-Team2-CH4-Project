// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * 앨리스 RMB 보조 공격 - 시계 토끼 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_AliceSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_AliceSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 실시간으로 타깃을 스캔하는 가상 함수 (앨리스 고유 락온/해킹 시스템 확장용) */
	void ScanHackingTargets();

	/** 회중시계 토끼 환영 투사체를 직선 사출하는 핵심 함수 */
	void FireClockRabbit();

	// 락온 체크 및 스캔용 타이머 핸들
	FTimerHandle LockOnTimerHandle;

	// 락온된 타깃들을 기억할 배열
	UPROPERTY()
	TArray<AActor*> TrackedTargets;

	/** 기획 스펙: 관통 피해량 (30.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Combat")
	float BaseDamage;

	/** 최종 대미지 및 인장/둔화 디버프를 타겟에게 주입할 GameplayEffect 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effect")
	TSubclassOf<UGameplayEffect> RabbitImpactEffectClass;
};