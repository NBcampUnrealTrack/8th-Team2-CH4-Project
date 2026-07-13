// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UAnimMontage;
class AFT_ProjectileBase; // 전용 투사체 포워드 디클레어 추가

/**
 * 앨리스 RMB 보조 공격 - 시계 토끼 환영 직선 관통 사출 스킬 클래스입니다.
 * 기획 명세: 경로상 적 관통, 피해량 30, 적중 대상 2초 둔화 및 시계 태엽 인장 새김, 쿨타임 10초.
 */
UCLASS()
class FIERYTALE_API UFT_AliceSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AliceSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    /** [기획 사양 완착]: 회중시계 토끼 환영 투사체를 전방 직선으로 빠르게 사출하는 핵심 함수 */
    void FireClockRabbit();
    
    /** 스킬 애니메이션이 정상 종료되었을 때 최종적으로 어빌리티를 정리 마감할 콜백 함수 */
    UFUNCTION()
    void OnSkillMontageFinished();

    /** 
     * 시전 도중 하드 CC기를 맞고 애니메이션이 파쇄당했을 때 
     * 선제 적용된 10초 쿨타임을 무결하게 반환하고 능력을 조기 종료하기 위한 수신용 동적 콜백 함수입니다.
     */
    UFUNCTION()
    void HandleSkillInterrupted();

protected:
    
    // 💡 노멀 어택처럼 공용 GE_Damage를 직접 꽂을 수 있도록 변수를 변경합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;
    
    /** [2번째 사양 완착]: 평타 무기 데이터와 공유하지 않는 보조 공격 '시계 토끼' 고유의 독립된 투사체 블루프린트 매핑 슬롯 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Alice Spec")
    TSubclassOf<AFT_ProjectileBase> ClockRabbitProjectileClass;

    /** 기획 스펙: 관통 기저 피해량 (기본값 30.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Alice Spec")
    float BaseDamage;

    /** [기획 연동 GE]: 관통된 적의 발밑에 시계 태엽 인장을 새기고 2초간 둔화시킬 딜러/디버프 융합형 이펙트 클래스 명세 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Effects")
    TSubclassOf<class UGameplayEffect> RabbitImpactEffectClass;

    /** 회중시계 토끼 환영을 사출할 때 본체가 재생할 고유 스킬 캐스팅 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
    TObjectPtr<UAnimMontage> AttackMontage;
};