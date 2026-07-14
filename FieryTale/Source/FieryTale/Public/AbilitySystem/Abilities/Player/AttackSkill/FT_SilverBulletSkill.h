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
    /** 1초 장전 채널링이 정상 만료되었을 때 은탄 사출 및 최종 종결을 집도하는 함수 */
    UFUNCTION()
    void FireSilverBullet();

    /** 
     * 투사체 사출 없이 능력을 즉시 취소 종료하기 위한 수신용 동적 콜백 함수입니다.
     */
    UFUNCTION()
    void HandleChannellingInterrupted();

protected:
    //  노멀 어택처럼 공용 GE_Damage를 직접 꽂을 수 있도록 변수를 통일합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    //  [투사체 클래스 에디터 노출 슬롯 배관]
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Projectile")
    TSubclassOf<class AFT_ProjectileBase> ProjectileClass;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Animation")
    TObjectPtr<UAnimMontage> SkillMontage;
    
    /** 1초 장전 메커니즘 안전 제어용 순정 타이머 핸들 */
    FTimerHandle ChannellingTimerHandle;

    /** 관통 은탄 적중 시 사출할 기본 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | RedRidingHood Spec")
    float BaseDamage;

    /** 은탄 발사 전 유지해야 하는 선행 장전 채널링 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | RedRidingHood Spec")
    float ChannellingDuration;
    
    /** 
     * 은탄 관통 적중 시 가동될 종합 명세서 GameplayEffect 클래스 
     * C++의 50.0f 데미지와 에디터의 2초 지속 50퍼센트 슬로우 디버프를 결합하는 통로
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> SilverBulletImpactEffectClass;
    
};