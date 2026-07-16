// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_SilverBulletSkill.generated.h"

/**
 * RMB 보조 공격 - 은빛 탄환 (1초 장전 후 강력한 관통 사격)
 */
UCLASS()
class FIERYTALE_API UFT_SilverBulletSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_SilverBulletSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 은탄 사출 함수입니다. */
    UFUNCTION()
    void FireSilverBullet();

    /** 채널링 중단 콜백입니다. */
    UFUNCTION()
    void HandleChannellingInterrupted();

protected:
    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    /** 투사체 클래스 */
    /** 발사할 투사체 클래스 (소프트 참조) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | SilverBullet Spec")
    TSoftClassPtr<class AFT_ProjectileBase> ProjectileClass;

    /** 슬로우 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> SlowEffectClass;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Animation")
    TSoftObjectPtr<UAnimMontage> SkillMontage;
    
    /** 장전 타이머 핸들 */
    FTimerHandle ChannellingTimerHandle;

    /** 기본 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | RedRidingHood Spec")
    float BaseDamage;

    /** 장전 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | RedRidingHood Spec")
    float ChannellingDuration;
    
    /** 타격 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> SilverBulletImpactEffectClass;
    
};