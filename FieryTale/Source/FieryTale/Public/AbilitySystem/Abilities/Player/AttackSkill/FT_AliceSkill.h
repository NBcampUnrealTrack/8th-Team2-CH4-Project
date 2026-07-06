// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UAnimMontage;

/**
 * 앨리스 RMB 보조 공격 - 시계 토끼 환영 관통 및 실시간 락온 스캔 시스템
 */
UCLASS()
class FIERYTALE_API UFT_AliceSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AliceSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 실시간으로 타깃을 스캔하는 가상 함수 */
    void ScanHackingTargets();

    /** 회중시계 토끼 환영 투사체를 직선 사출하는 핵심 함수 */
    void FireClockRabbit();
    
    /** 스킬 애니메이션이 정상 종료되었을 때 최종 쿨타임을 마킹하고 어빌리티를 마감할 콜백 함수 */
    UFUNCTION()
    void OnSkillMontageFinished();

    /** 
     * 💡 [심볼 해결 완료]: 시전 도중 하드 CC기를 맞고 애니메이션이 파쇄당했을 때 
     * 쿨타임 페널티 없이 즉시 능력을 조기 종료하기 위한 수신용 동적 콜백 함수입니다.
     */
    UFUNCTION()
    void HandleSkillInterrupted();

protected:
    /** 락온 체크 및 타깃 지속 스캔용 순정 타이머 핸들 */
    FTimerHandle LockOnTimerHandle;

    /** 락온 추적 시스템에 포착된 타깃 액터들을 안전하게 기억할 언리얼 가비지 컬렉션 가드 배열 */
    UPROPERTY()
    TArray<TObjectPtr<AActor>> TrackedTargets;

    /** 기획 스펙: 관통 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Alice Spec")
    float BaseDamage;

    /** 최종 대미지 및 인장 둔화 디버프를 타겟에게 주입할 핵심 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> RabbitImpactEffectClass;

    /** 회중시계 토끼 환영을 사출할 때 본체가 재생할 고유 스킬 캐스팅 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;
};