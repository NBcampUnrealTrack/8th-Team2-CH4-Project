// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_ChargedShotSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UAnimMontage;

/**
 * 알라딘 [RMB] 보조 공격 - 지니의 압착 어빌리티 클래스
 * 전방 지정 원형 범위에 지니의 거대한 주먹을 호출하여 광역 피해 및 방사형 넉백을 선사합니다.
 */
UCLASS()
class FIERYTALE_API UFT_ChargedShotSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_ChargedShotSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 광역 대미지 및 넉백 로직을 실행합니다. */
    void ExecuteGeniusCrushLogic(const FVector& TargetCenterLocation, AFTPlayerCharacterBase* InCharacter);

    /** 몽타주 종료 콜백입니다. */
    UFUNCTION()
    void OnFireMontageFinished();

protected:
    // --- 고유 스펙 ---
    /** 광역 피해량 (기본값 50.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float BaseDamage;

    /** 넉백 강도 (기본값 800.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float KnockbackForce;

    /** 사거리 (기본값 600.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float SkillRange;

    /** 타격 반경 (기본값 250.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float SkillRadius;

    // --- 이펙트 클래스 ---
    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    // --- 비주얼 클래스 ---
    /** 스킬 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TObjectPtr<UAnimMontage> FireMontage;
};