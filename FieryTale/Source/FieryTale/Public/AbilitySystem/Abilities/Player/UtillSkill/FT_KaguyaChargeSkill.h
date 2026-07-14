// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_KaguyaChargeSkill.generated.h"

class AFTPlayerCharacterBase;

/**
 * 가구야 공주 Shift 기술 - 대나무 숲의 숨결 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaChargeSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_KaguyaChargeSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 지속 시간이 만료되었을 때 호출되는 콜백입니다. */
    UFUNCTION()
    void OnChargeFinished();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
    TObjectPtr<class UAnimMontage> SkillMontage;

    /** 지속 시간 타이머 핸들 */
    FTimerHandle BambooGroveDurationTimerHandle;

    // --- 고유 스펙 ---
    /** 안개 영역 생성 반경 (기본 450.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
    float BambooGroveRadius;

    /** 영역 유지 시간 (기본 4.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
    float BambooGroveDuration;

    // 블루프린트에서 할당할 영역 액터 클래스입니다.
    /** 영역 액터 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Spec")
    TSubclassOf<AActor> BambooGroveAreaClass;

    // --- 이펙트 클래스 ---
    /** 은신 버프 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> ConcealmentEffectClass;
};