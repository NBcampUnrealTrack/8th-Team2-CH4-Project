// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "FT_AliceUltimateAbility.generated.h"

/**
 * 시계 토끼 앨리스 궁극기: 아직 6시야! 무대는 끝나지 않아! (광역 시간 왜곡)
 */
UCLASS()
class FIERYTALE_API UFT_AliceUltimateAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()
    
public:
	UFT_AliceUltimateAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// [보정 완료] 타이머 핸들 바인딩과 댕글링 포인터 크래시 방지를 위해 인자를 완전히 배제하고 UFUNCTION으로 격상합니다.
	/** 1초 채널링 락이 만료되는 즉시 앨리스 본인의 움직임을 선제 봉쇄 해제하는 안전 콜백 함수 */
	UFUNCTION()
	void ReleaseAlice();

protected:
	// --- 앨리스 궁극기 고유 스펙 ---
	/** 자신 중심 시간 정지 영역 반지름 (기본값 600.f / 6m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
	float TimeStopRadius;

	// --- 연동할 GameplayEffect(GE) 라인업 ---
	/** 영역 내부의 적들에게 2초간 '기절(Stunned)' 및 이펙트를 주입할 데미지/디버프 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> TimeStopDebuffEffectClass;

	/** 앨리스 본인의 선행 채널링 해제 시점을 통제할 내부 지속 타이머 핸들 */
	FTimerHandle AliceReleaseTimerHandle;
};