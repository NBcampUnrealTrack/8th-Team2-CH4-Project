// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h" // FActiveGameplayEffectHandle 참조를 위한 순정 헤더 포함
#include "FT_NormalAttack.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * FieryTale 영웅들의 공용 마스터 평타 어빌리티 시스템 헤더입니다.
 * 히트스캔, 투사체, 근접 무기 유형을 마스터 데이터 에셋 스펙에 맞춰 실시간 분기 처리합니다.
 */
UCLASS()
class FIERYTALE_API UFT_NormalAttack : public UFT_GameplayAbility
{
    GENERATED_BODY()
    
public:
    UFT_NormalAttack();
    
    // GAS 순정 최선행 검증 관문 오버라이드
    // 이 함수가 true를 사출해야만 실질적인 ActivateAbility 배관이 개통되어 평타가 나갑니다.
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    
    // [M-2 완치 보장] 인터럽트 캔슬(기절 등)을 맞더라도 부착된 이속 패널티 GE를 무결하게 소거하는 방어선 관문입니다.
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 공격 몽타주 완료 또는 인터럽트 시 내부 정리 관문으로 이어질 안전 콜백 함수 */
    UFUNCTION()
    void OnAttackMontageFinished();
    
    /** 실제 격발 타이밍에 호출될 판정 엔진 핵심 함수 (애님 노티파이 및 블루프린트 연동 호환) */
    UFUNCTION(BlueprintCallable, Category = "FieryTale|Combat")
    void ExecuteWeaponHitDetection(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter);

    /** [LineTrace 타입] 히트스캔 즉시 발사 및 헤드샷 판정 연산 */
    void PerformLineTraceLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward);

    /** [Projectile 타입] 투사체 사출 및 콤보 점사 타이머 제어 구역 */
    void SpawnProjectileLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward);

    /** [Melee 타입] 박스 트레이스 기반 근접 타격 볼륨 스캔 구역 */
    void PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter);
    
    // --- 연동할 GameplayEffect(GE) 라인업 ---
    /** 최종 데미지 수치를 주입하여 타겟의 AttributeSet에 전달할 기본 피해량 이펙트 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> BaseDamageEffectClass;

    // =========================================================================
    // [M-2 속도 패널티 이관용 GAS 에셋 슬롯 및 핸들 장부 완착]
    // =========================================================================
    
    /** 평타 가동 중 캐릭터에게 적용할 이동속도 배율 감산 GameplayEffect 에셋 클래스입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> MovementPenaltyGameplayEffectClass;

private:
    /** 평타 연사 도중 강제 인터럽트 캔슬 시 투사체 무한 유실을 차단하기 위한 멤버 타이머 핸들 변수 */
    FTimerHandle BurstTimerHandle;

    /** 런타임에 부여된 속도 패널티 GE를 종료 시 무결하게 걷어내기 위해 백업해두는 액티브 핸들입니다. */
    FActiveGameplayEffectHandle MovementPenaltyActiveHandle;
};