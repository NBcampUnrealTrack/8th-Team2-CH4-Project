// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "FT_AliceStealthDashSkill.generated.h"

class AFTPlayerCharacterBase;
class UGameplayEffect;

/**
 * 앨리스 Shift 기술 - 토끼의 물약 신체 축소 가속 어빌리티 시스템입니다.
 * 은신 개념은 기획 사양에 따라 전면 제외되었으며, 히트박스 축소와 이동 가속에 집중합니다.
 */
UCLASS()
class FIERYTALE_API UFT_AliceStealthDashSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AliceStealthDashSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 버프 지속 시간 만료 시 호출되는 콜백입니다. */
    UFUNCTION()
    void OnShrinkBuffFinished();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
    TObjectPtr<class UAnimMontage> SkillMontage;

    /** 버프 지속 타이머 핸들 */
    FTimerHandle ShrinkBuffDurationTimerHandle;

    // --- 앨리스 신체 축소 고유 스펙 ---
    /** 버프 지속 시간 (기본값 2.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float MaxDuration;

    /** 목표 크기 비율 (기본값 0.5f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float TargetMeshScale;

    // 이동 속도 제어를 위해 이펙트를 사용합니다.
    /** 대시 가속 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice GAS")
    TSubclassOf<UGameplayEffect> DashSpeedGameplayEffectClass;

    /** 대시 가속 이펙트 핸들 */
    FActiveGameplayEffectHandle DashSpeedActiveHandle;
};