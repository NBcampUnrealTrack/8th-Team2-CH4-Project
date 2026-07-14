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
    /** 💡 [기획서 사양 부합 핵심 배관]: 전방 범위 내의 적들을 탐색하고 원형 방사형 넉백 및 대미지를 연산하는 마스터 서버 로직 */
    void ExecuteGeniusCrushLogic(const FVector& TargetCenterLocation, AFTPlayerCharacterBase* InCharacter);

    /** 지니의 주먹 사출 모션이 완전히 마감되었을 때 수명주기를 닫아줄 콜백 */
    UFUNCTION()
    void OnFireMontageFinished();

protected:
    // --- 알라딘 우클릭 고유 기획 스펙 장부 ---
    /** 기획 스펙: 지니의 주먹 적중 시 가할 광역 고정 피해량 (명세서 사양: 50.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float BaseDamage;

    /** 기획 스펙: 주먹 중심부에서 바깥쪽으로 적들을 강하게 밀쳐내기 위한 방사형 넉백 물리 강도 (기본값: 800.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float KnockbackForce;

    /** 💡 [기획 명세 추가]: 알라딘 전방으로 지니의 주먹이 떨어질 중심점 거리 (기본값: 600.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float SkillRange;

    /** 💡 [기획 명세 추가]: 지니의 주먹 종말 타격 원형 반경 범위 (기본값: 250.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Aladdin Spec")
    float SkillRadius;

    // --- 연동할 GameplayEffect(GE) 라인업 ---
    /** 대미지 주입 및 상태 이상 정산을 가동할 확정 피해 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    // --- 알라딘 격발 비주얼 자산 슬롯 ---
    /** 마우스 우클릭 RMB 즉시 격발 시 본체가 재생할 지니 호출 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TObjectPtr<UAnimMontage> FireMontage;
};