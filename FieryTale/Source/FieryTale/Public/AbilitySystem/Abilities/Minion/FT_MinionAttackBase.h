// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FT_MinionAttackBase.generated.h"

class UAnimMontage;
class UGameplayEffect;
class AFT_ProjectileBase; // 마스터 투사체 전방 선언을 통한 빌드 다이어트

/**
 * FieryTale 프로젝트 내 모든 미니언(근접/원거리)의 기본 공격 메커니즘을 관장하는
 * 마스터 베이스 GAS 공격 어빌리티입니다.
 */
UCLASS()
class FIERYTALE_API UFT_MinionAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UFT_MinionAttackBase();

protected:
    /** 브레인 어빌리티나 AI 명령에 의해 공격이 격발될 때 진입하는 순정 관문 */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** 공격 애니메이션이 끝나거나 취소될 때 안전하게 배관을 닫아줄 관문 */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    
    /** 미니언 종류별 평타 휘두르기 / 투사체 발사 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;

    /** 타격 성공 시 적의 체력을 깎아내릴 GAS 데미지 계산서 계산 규격 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | GAS")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
    
    /** 
     * 원거리 미니언일 경우 사출할 투사체 블루프린트 자산 슬롯 
     * (근접 미니언은 에디터에서 None으로 비워두면 즉시 타격 연산으로 자동 분기됩니다.)
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | DesignData")
    TSubclassOf<AFT_ProjectileBase> ProjectileClass;

    /** 미니언 고유의 기본 공격 피해량 수치 명세 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | DesignData")
    float BaseDamage;
    
    /** 
     * 순정 UAbilityTask_WaitGameplayEvent 규격에 맞춘 이벤트 바인딩 함수 
     * 기존의 FGameplayTag 매개변수를 제거하여 시그니처를 일치시켰습니다.
     */
    UFUNCTION()
    void OnMontageTargetedEvent(FGameplayEventData EventData);

    UFUNCTION()
    void OnMontageCompletedOrCancelled();
};