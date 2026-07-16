// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "FT_AladdinFlySkill.generated.h"

class AFTPlayerCharacterBase;
class UGameplayEffect;

/**
 * 알라딘 Shift 기술 - 양탄자 기류 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_AladdinFlySkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AladdinFlySkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 비행 지속 시간이 만료되었을 때 호출되는 콜백입니다. */
    UFUNCTION()
    void OnFlyDurationExpired();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TSoftObjectPtr<class UAnimMontage> SkillMontage;

    /** 비행 유지 타이머 핸들 */
    FTimerHandle FlyDurationTimerHandle;

    // --- 고유 스펙 ---
    /** 비행 지속 시간 (기본값 5.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float FlyDuration;

    // GAS 이동 속도 제어를 위해 이펙트를 사용합니다.
    /** 이동 속도 감소 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin GAS")
    TSubclassOf<UGameplayEffect> FlyMovementPenaltyGameplayEffectClass;

    /** 이동 속도 감소 이펙트 핸들 */
    FActiveGameplayEffectHandle FlyMovementPenaltyActiveHandle;
};