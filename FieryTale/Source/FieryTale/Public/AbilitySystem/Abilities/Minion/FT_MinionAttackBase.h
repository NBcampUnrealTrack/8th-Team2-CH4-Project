// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FT_MinionAttackBase.generated.h"

class UAnimMontage;
class UGameplayEffect;
class AFT_ProjectileBase;

/**
 * 모든 미니언의 기본 공격 어빌리티입니다.
 */
UCLASS()
class FIERYTALE_API UFT_MinionAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UFT_MinionAttackBase();

protected:
    /** 어빌리티가 활성화될 때 호출됩니다. */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** 어빌리티가 종료될 때 호출됩니다. */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    
    /** 공격 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;

    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | GAS")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
    
    UPROPERTY()
    class UAbilityTask_WaitGameplayEvent* ActiveWaitEventTask;
    
    /** 
     * 투사체 클래스. 설정되지 않은 경우 근접 공격으로 처리됩니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | DesignData")
    TSubclassOf<AFT_ProjectileBase> ProjectileClass;

    /** 기본 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | DesignData")
    float BaseDamage;
    
    /** 몽타주 내 애니메이션 노티파이 발생 시 이벤트 처리 */
    UFUNCTION()
    void OnMontageTargetedEvent(FGameplayEventData EventData);

    /** 애니메이션 종료 시 호출되는 콜백 */
    UFUNCTION()
    void OnMontageCompletedOrCancelled();
    
    FTimerHandle DebugNoMontageTimerHandle;
};