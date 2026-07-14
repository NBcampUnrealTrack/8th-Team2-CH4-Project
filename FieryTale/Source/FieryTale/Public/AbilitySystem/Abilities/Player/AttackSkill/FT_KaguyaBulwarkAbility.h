// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "FT_KaguyaBulwarkAbility.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UGameplayEffect;

/**
 * 가구야 공주 RMB 강공격 - 봉래의 탄환 방벽 어빌리티 시스템입니다.
 * 전방 120도 각도의 모든 피해를 소멸시키며 아군 딜러진에게 완벽한 프리딜 구도를 선사하는 핵심 방어 기술입니다.
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaBulwarkAbility : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_KaguyaBulwarkAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 방벽 지속 시간 만료 시 호출되는 콜백입니다. */
    UFUNCTION()
    void ExpireBulwark();

    /** 가드 중단 콜백입니다. */
    UFUNCTION()
    void OnGuardInterrupted();

protected:
    /** 방벽 지속 타이머 핸들 */
    FTimerHandle BulwarkDurationTimerHandle;

    // --- 고유 스펙 ---
    /** 최대 지속 시간 (기본값 4.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
    float MaxDuration;

    /** 이동 속도 감소 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark GAS")
    TSubclassOf<UGameplayEffect> MovementPenaltyGameplayEffectClass;

    /** 이동 속도 감소 이펙트 핸들 */
    FActiveGameplayEffectHandle MovementPenaltyActiveHandle;
};