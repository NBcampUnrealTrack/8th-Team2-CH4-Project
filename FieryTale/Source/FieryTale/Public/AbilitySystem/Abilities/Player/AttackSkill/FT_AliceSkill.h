// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UAnimMontage;
class AFT_ProjectileBase;

/**
 * 앨리스 RMB 보조 공격 - 시계 토끼 환영 직선 관통 사출 스킬 클래스입니다.
 * 기획 명세: 경로상 적 관통, 피해량 30, 적중 대상 2초 둔화 및 시계 태엽 인장 새김, 쿨타임 10초.
 */
UCLASS()
class FIERYTALE_API UFT_AliceSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AliceSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    /** 토끼 환영 투사체 사출 함수입니다. */
    void FireClockRabbit();
    
    /** 애니메이션 종료 콜백입니다. */
    UFUNCTION()
    void OnSkillMontageFinished();

    /** 어빌리티 중단 콜백입니다. */
    UFUNCTION()
    void HandleSkillInterrupted();

protected:
    
    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;
    
    /** 투사체 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Alice Spec")
    TSubclassOf<AFT_ProjectileBase> ClockRabbitProjectileClass;

    /** 기본 피해량 (기본값 30.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Alice Spec")
    float BaseDamage;

    /** 타격 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> RabbitImpactEffectClass;

    /** 스킬 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;
};