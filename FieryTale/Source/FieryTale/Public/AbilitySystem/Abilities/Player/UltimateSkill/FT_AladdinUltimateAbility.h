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

protected:
    // 스킬 키가 눌릴 때마다 호출되는 GAS 기본 관문
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // 지니가 전방을 강타하는 실질적인 물리 박스 오버랩 스캔 함수
    void ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex);

    // ◄◄◄ [콤보 타임아웃 배관 완착] ◄◄◄
    /** 3초 콤보 제한 시간이 만료되었을 때 좀비 태그와 스택을 무조건 청소할 수거용 콜백 함수 */
    UFUNCTION()
    void ResetComboState();

protected:
    // --- 알라딘 궁극기 고유 스펙 ---
    /** 직선 타격 범위 사거리 수치 (기본값 1500.f / 15m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float AttackRange;

    /** 직선 박스의 좌우 반폭 수치 (기본값 300.f / 총 너비 6m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float AttackWidth;

    /** 기획서 반영: 지니 환영 강타 적중 시 사출할 기본 피해량 수치 (기본값 50.f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float BaseDamageValue;

    // --- 연동할 GameplayEffect(GE) Lineup ---
    /** 피해 연산기(GEEC_Damage)를 가동할 확정 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

protected:
    /** 콤보 입력 대기 윈도우 유효 시간을 통제할 3초 한계선 타이머 핸들 */
    FTimerHandle ComboTimeoutTimerHandle;

    /** [상태 머신 트래커] 현재 시전 중인 소원의 콤보 단계 카운트 (1 -> 2 -> 3타) */
    int32 CurrentWishCount;
    
    /** 알라딘이 시전할 수 있는 최대 소원(콤보) 한계치 */
    static constexpr int32 MaxWishes = 3;
};