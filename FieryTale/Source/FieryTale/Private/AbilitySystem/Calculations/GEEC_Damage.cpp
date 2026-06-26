// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Calculations/GEEC_Damage.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"

// --------------------------------------------------------------------------------------
// [GAS 내부 규칙] 계산기 안에서 실시간으로 캡처해서 감시할 피해자(Target)의 속성 구조체입니다.
// --------------------------------------------------------------------------------------
struct FT_DamageStatCapture
{
    // 피해자의 보호막과 체력만 실시간으로 감시합니다.
    DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Health);

    FT_DamageStatCapture()
    {
        // 피해자(Target)의 AttributeSet으로부터 속성들을 실시간 매핑합니다.
        DEFINE_ATTRIBUTE_CAPTUREDEF(UFT_AttributeSet, Shield, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UFT_AttributeSet, Health, Target, false);
    }
};

static const FT_DamageStatCapture& DamageStatCapture()
{
    static FT_DamageStatCapture DCapture;
    return DCapture;
}

UGEEC_Damage::UGEEC_Damage()
{
    // 이 계산기가 구동될 때 보호막과 체력 두 가지 속성만 캡처하도록 등록합니다.
    RelevantAttributesToCapture.Add(DamageStatCapture().ShieldDef);
    RelevantAttributesToCapture.Add(DamageStatCapture().HealthDef);
}

void UGEEC_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& ExecutionOutputs) const
{
    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

    // ◄◄◄ [수정 포인트 1] 크래시 방지 방어 코드 추가
    // 가해자나 피해자의 ASC가 유효하지 않다면 연산을 수행하지 않고 즉시 탈출합니다.
    if (!SourceASC || !TargetASC)
    {
        return;
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // 1. 평타 C++ 코드에서 'SetByCaller' 우체통으로 보내준 기저 대미지(30 또는 60)를 꺼냅니다.
    float BaseDamage = FMath::Max<float>(Spec.GetSetByCallerMagnitude(FName("Damage"), false, -1.0f), 0.0f);

    // 2. 피해자의 현재 보호막(Shield) 수치와 체력(Health) 수치를 안전하게 캡처해옵니다.
    float CurrentShield = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatCapture().ShieldDef, FAggregatorEvaluateParameters(), CurrentShield);
    CurrentShield = FMath::Max<float>(CurrentShield, 0.0f);

    float CurrentHealth = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatCapture().HealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
    CurrentHealth = FMath::Max<float>(CurrentHealth, 0.0f);
    
    // [보호막 선차감 메커니즘] 방어력이 없으므로 기저 대미지가 그대로 적용됩니다.
    float DamageToShield = 0.f;
    float DamageToHealth = 0.f;

    if (CurrentShield > 0.f)
    {
        // 보호막이 대미지보다 더 많거나 같아서 버텨낼 수 있는 경우
        if (CurrentShield >= BaseDamage)
        {
            DamageToShield = BaseDamage;
            DamageToHealth = 0.f; // 체력은 안전함
        }
        // 보호막이 대미지보다 부족해서 깨져버리는 경우
        else
        {
            DamageToShield = CurrentShield; // 있는 보호막만 다 깨부수고
            DamageToHealth = BaseDamage - CurrentShield; // 남은 잔여 대미지가 체력으로 흘러 들어감
        }
    }
    else
    {
        // 보호막이 애초에 없다면 모든 대미지가 체력으로 다이렉트 전사
        DamageToShield = 0.f;
        DamageToHealth = BaseDamage;
    }

    // ◄◄◄ [수정 포인트 2] 오버킬(Overkill) 클램핑 연산 추가
    // 최종적으로 가해질 체력 대미지가 현재 남은 체력보다 크다면, 남은 체력만큼만 대미지를 주도록 제한합니다.
    if (DamageToHealth > CurrentHealth)
    {
        DamageToHealth = CurrentHealth;
    }

    // --------------------------------------------------------------------------------------
    // [출력 단계] 최종 연산된 결과를 피해자의 AttributeSet에 마이너스(-) 값으로 전달
    // --------------------------------------------------------------------------------------
    // 계산된 만큼 보호막(Shield) 어트리뷰트를 차감시킵니다.
    if (DamageToShield > 0.f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatCapture().ShieldProperty, EGameplayModOp::Additive, -DamageToShield));
    }

    // 계산된 만큼 최종적으로 체력(Health) 어트리뷰트를 차감시킵니다.
    if (DamageToHealth > 0.f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatCapture().HealthProperty, EGameplayModOp::Additive, -DamageToHealth));
    }
}