// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_ChargedShotSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UAnimMontage; // ◄ 애니메이션 몽타주 전방 선언 주입으로 컴파일 속도 최적화

/**
 * 알라딘 RMB 보조 공격 - 지니의 압착 어빌리티 시스템 (차징 샷)
 */
UCLASS()
class FIERYTALE_API UFT_ChargedShotSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_ChargedShotSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // ◄◄◄ [인풋 릴리즈 배관 완착] 마우스 우클릭 버튼을 떼는 순간 캐릭터 제어 허브에서 직통 격발할 핵심 사출 인터페이스
    /** 1초 차징 여부를 판단하여 지니의 폭발 주먹 투사체를 사출하고 쿨다운을 적용시키는 핵심 함수 */
    UFUNCTION(BlueprintCallable, Category = "FieryTale|Aladdin Spec")
    void FireChargedShot();

protected:
    // =========================================================================
    // [GAS 비동기 애니메이션 태스크 콜백 인터페이스 완착]
    // 격발 몽타주 재생이 끝나거나 중도 취소되었을 때 안전하게 어빌리티를 종료할 관문입니다.
    // =========================================================================
    /** 지니의 주먹 사출 모션이 완전히 마감되었을 때 수명주기를 닫아줄 콜백 */
    UFUNCTION()
    void OnFireMontageFinished();

protected:
    /** 차징이 시작된 절대 타임스탬프 기록 보관소 */
    float ChargeStartTime;

    // --- 알라딘 우클릭 고유 스펙 ---
    /** 기획 스펙: 지니의 주먹 폭발 적중 시 사출할 고정 피해량 (기본값 50.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float BaseDamage;

    /** 기획 스펙: 폭발 중심부에서 바깥쪽으로 적들을 밀어내기 위한 넉백 물리 강도 (기본값 800.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Aladdin Spec")
    float KnockbackForce;

    // --- 연동할 GameplayEffect(GE) 라인업 ---
    /** 대미지 주입 및 원형 폭발 계산기(GEEC_Damage)를 가동할 확정 피해 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    // --- 알라딘 격발 비주얼 자산 슬롯 완착 ---
    /** 1초 차징 완수 후 마우스를 떼고 주먹을 방출할 때 본체가 재생할 강력한 사출 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Visual")
    TObjectPtr<UAnimMontage> FireMontage;
};