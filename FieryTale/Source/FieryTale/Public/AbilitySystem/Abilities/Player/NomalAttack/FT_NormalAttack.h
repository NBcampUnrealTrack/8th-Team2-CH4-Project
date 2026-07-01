// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_NormalAttack.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * FieryTale 영웅들의 공용 마스터 평타 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_NormalAttack : public UFT_GameplayAbility
{
    GENERATED_BODY()
    
public:
    UFT_NormalAttack();
    
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
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

private:
    // ◄◄◄ [완벽 보정] .cpp 본문에서 메모리 폭산 및 영구 속도 유실을 청소하기 위한 실시간 안전장치 보관소
    /** 평타 연사(점사) 도중 강제 인터럽트 캔슬 시 투사체 무한 유실을 차단하기 위한 멤버 타이머 핸들 변수 */
    FTimerHandle BurstTimerHandle;

    /** 평타 가감 배율 누적 오차로 인한 무브먼트 영구 왜곡을 방쇄하기 위한 순정 최대 속도 백업 보관소 */
    float OriginalMaxWalkSpeed;
};