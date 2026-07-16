// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "FT_AladdinUltimateAbility.generated.h"

/**
 * 알라딘 궁극기: 세 가지 소원 (지니 환영 3단 강타)
 */
UCLASS()
class FIERYTALE_API UFT_AladdinUltimateAbility : public UFT_UltimateGameplayAbility
{
    GENERATED_BODY()
    
public:
    UFT_AladdinUltimateAbility();

    // 콤보 가동 중일 경우 코스트 검증을 우회하기 위해 CanActivateAbility를 오버라이드합니다.
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    // 스킬 키가 눌릴 때마다 호출되는 GAS 기본 관문
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // 전방 강타 판정 및 데미지 적용 함수입니다.
    void ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex);

    /** 콤보 제한 시간 만료 시 호출되는 콜백입니다. */
    UFUNCTION()
    void ResetComboState();

    /** 연속 입력 감지 콜백입니다. */
    UFUNCTION()
    void OnComboInputPressed(float TimeWaited);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
    TSoftObjectPtr<class UAnimMontage> SkillMontage;

protected:
    // --- 알라딘 궁극기 고유 스펙 ---
    /** 사거리 수치 (기본값 1500.f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float AttackRange;

    /** 공격 너비 수치 (기본값 300.f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float AttackWidth;

    /** 기본 피해량 수치 (기본값 50.f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float BaseDamageValue;

    // --- 연동할 GameplayEffect(GE) 클래스 ---
    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

protected:
    /** 콤보 제한 시간 타이머 핸들 */
    FTimerHandle ComboTimeoutTimerHandle;

    /** 현재 콤보 카운트 */
    int32 CurrentWishCount;
    
    /** 최대 콤보 카운트 */
    static constexpr int32 MaxWishes = 3;
};