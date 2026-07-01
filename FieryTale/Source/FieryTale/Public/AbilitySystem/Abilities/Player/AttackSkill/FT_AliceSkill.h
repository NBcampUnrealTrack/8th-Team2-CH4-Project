// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * 앨리스 RMB 보조 공격 - 시계 토끼 환영 관통 및 실시간 락온 스캔 시스템
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
	/** 실시간으로 타깃을 스캔하는 가상 함수 (앨리스 고유 락온/해킹 시스템 확장용 거점) */
	void ScanHackingTargets();

	/** 회중시계 토끼 환영 투사체를 직선 사출하는 핵심 함수 */
	void FireClockRabbit();

protected:
	/** 락온 체크 및 타깃 지속 스캔용 순정 타이머 핸들 */
	FTimerHandle LockOnTimerHandle;

	/** 락온 추적 시스템에 포착된 타깃 액터들을 안전하게 기억할 언리얼 가비지 컬렉션 가드 배열 */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> TrackedTargets;

	// --- 앨리스 보조 공격 고유 스펙 ---
	/** 기획 스펙: 관통 피해량 (기본값 30.0f) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
	float BaseDamage;

	// --- 연동할 GameplayEffect(GE) 라인업 ---
	/** 최종 대미지 및 인장(Target_Insignia)/둔화(Debuff_Slow) 디버프를 타겟에게 주입할 핵심 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> RabbitImpactEffectClass;
};