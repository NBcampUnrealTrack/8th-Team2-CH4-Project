// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FT_MinionAttackBase.generated.h"

class UAnimMontage;
class UGameplayEffect;

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

    // =========================================================================
    // [기획 및 에디터 밸런스 튜닝 데이터 에셋 슬롯]
    // =========================================================================
    /** 미니언 종류별 평타 휘두르기 / 투사체 발사 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;

    /** 타격 성공 시 적의 체력을 깎아내릴 GAS 데미지 계산서 계산 규격 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Attack | GAS")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // =========================================================================
    // [GAS 원자적 애니메이션 태스크 연동 배관]
    // =========================================================================
    /** 몽타주 재생 및 애니메이션 노티파이(타격 타이밍)를 C++ 단에서 정밀 제어하기 위한 바인딩 함수 */
    UFUNCTION()
    void OnMontageTargetedEvent(FGameplayTag EventTag, FGameplayEventData EventData);

    UFUNCTION()
    void OnMontageCompletedOrCancelled();
};