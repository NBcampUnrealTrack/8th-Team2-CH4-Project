// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h" // ◄ FActiveGameplayEffectHandle 참조 배관망을 위해 인클루드 완착
#include "FT_AladdinFlySkill.generated.h"

class AFTPlayerCharacterBase;
class UGameplayEffect; // ◄ 전방 선언을 통해 컴파일 속도 최적화

/**
 * 알라딘 Shift 기술 - 양탄자 기류 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_AladdinFlySkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AladdinFlySkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 5초 비행 지속 시간이 만료되었을 때 어빌리티 수명 주기를 닫아줄 타이머 콜백 함수 */
    UFUNCTION()
    void OnFlyDurationExpired();

protected:
    /** 5초 유지를 정밀하게 제어할 순정 타이머 핸들 */
    FTimerHandle FlyDurationTimerHandle;

    // --- 알라딘 비행 고유 스펙 ---
    /** 양탄자 비행이 유지될 총 지속 시간 (기본값 5.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float FlyDuration;

    // =========================================================================
    // 💡 [이속 장부 파괴 릭 완치 배관]: 원시 변수 변조 하드코딩 방식을 전면 폐기하고,
    // 평타 및 상대방 CC 체계와 100% 동기화되는 GAS 순정 이동 속도 제어선으로 개통합니다.
    // =========================================================================
    /** 비행 전개 중 알라딘 본인의 이동 속도를 30% 감산 제어할 전용 둔화 이펙트 클래스 슬롯 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin GAS")
    TSubclassOf<UGameplayEffect> FlyMovementPenaltyGameplayEffectClass;

    /** 부착된 이속 패널티 이펙트를 종료/취소 시점에 오차 없이 걷어내기 위한 GAS 무결성 활성 핸들 */
    FActiveGameplayEffectHandle FlyMovementPenaltyActiveHandle;
};