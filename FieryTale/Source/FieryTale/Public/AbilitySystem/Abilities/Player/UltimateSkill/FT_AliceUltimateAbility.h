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
protected:
	/** 타이머 만료 시 어빌리티를 종료하는 콜백 함수입니다. */
	UFUNCTION()
	void ReleaseAlice();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Animation")
	TSoftObjectPtr<class UAnimMontage> SkillMontage;

	// --- 앨리스 궁극기 고유 스펙 ---
	/** 자신 중심 시간 정지 영역 반지름 (기본값 600.f / 6m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
	float TimeStopRadius;

	// --- 연동할 GameplayEffect(GE) 클래스 ---
	/** 영역 내부의 적들에게 '기절(Stunned)' 디버프를 주입할 이펙트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
	TSubclassOf<class UGameplayEffect> TimeStopDebuffEffectClass;

	/** 앨리스 채널링 해제 시점을 통제할 타이머 핸들 */
	FTimerHandle AliceReleaseTimerHandle;
};