// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Calculations/GEEC_Damage.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"

struct FT_DamageStatCapture
{
    // 계산기 내부에서 실시간으로 추적하고 감시할 대상의 보호막과 체력 속성을 선언합니다
    DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Health);

    FT_DamageStatCapture()
    {
        // 타깃의 AttributeSet으로부터 보호막과 체력 수치를 캡처하여 매핑합니다
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
    // 이 계산기 인스턴스가 실행될 때 캡처 구조체에 정의된 어트리뷰트들을 감시 목록에 등록합니다
    RelevantAttributesToCapture.Add(DamageStatCapture().ShieldDef);
    RelevantAttributesToCapture.Add(DamageStatCapture().HealthDef);
}

void UGEEC_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& ExecutionOutputs) const
{
    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
   
    // 공격자나 피격자의 능력 시스템 컴포넌트가 유효하지 않다면 연산을 수행하지 않고 조기 종료합니다
    if (!SourceASC || !TargetASC)
    {
        return;
    }
    
    // AOS 멀티플레이 환경을 고려한 블루 팀 대 레드 팀 진영 태그 피아식별을 수행합니다
    FGameplayTag BlueTeamTag = FGameplayTag::RequestGameplayTag(FName("Team.Blue"));
    FGameplayTag RedTeamTag = FGameplayTag::RequestGameplayTag(FName("Team.Red"));

    // 공격자와 피격자가 동일한 팀 태그를 들고 있다면 아군 오사로 간주하여 대미지 계산을 무효화합니다
    if ((SourceASC->HasMatchingGameplayTag(BlueTeamTag) && TargetASC->HasMatchingGameplayTag(BlueTeamTag)) ||
        (SourceASC->HasMatchingGameplayTag(RedTeamTag) && TargetASC->HasMatchingGameplayTag(RedTeamTag)))
    {
        return;
    }
    
    // 피격자가 가구야의 반격 가드 태세 버프 태그를 보유하고 있는지 검증합니다
    FGameplayTag CounterTag = FGameplayTag::RequestGameplayTag(FName("State.Buff.CounterReady"));
    if (TargetASC->HasMatchingGameplayTag(CounterTag))
    {
        // 반격 태세인 경우 모든 타격을 튕겨내는 기획 스펙에 맞추어 대미지 연산을 즉시 중단합니다
        return; 
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // 어빌리티나 무기 데이터 에셋에서 SetByCaller 우체통에 담아 보낸 기저 대미지 수치를 추출합니다
    float BaseDamage = FMath::Max<float>(Spec.GetSetByCallerMagnitude(FName("Damage"), false, -1.0f), 0.0f);
    
    // 피격자의 현재 보호막 수치와 체력 수치를 안전하게 계산 매개변수로부터 수집합니다
    float CurrentShield = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatCapture().ShieldDef, FAggregatorEvaluateParameters(), CurrentShield);
    CurrentShield = FMath::Max<float>(CurrentShield, 0.0f);

    float CurrentHealth = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatCapture().HealthDef, FAggregatorEvaluateParameters(), CurrentHealth);
    CurrentHealth = FMath::Max<float>(CurrentHealth, 0.0f);
    
    float DamageToShield = 0.f;
    float DamageToHealth = 0.f;

    // 보호막이 존재할 경우 체력보다 우선적으로 대미지를 감산하는 선차감 연산을 수행합니다
    if (CurrentShield > 0.f)
    {
        // 보호막 수치가 기저 대미지보다 크거나 같아서 모든 피해를 흡수할 수 있는 경우
        if (CurrentShield >= BaseDamage)
        {
            DamageToShield = BaseDamage;
            DamageToHealth = 0.f;
        }
        // 보호막 수치가 대미지보다 부족하여 파괴되고 잔여 대미지가 남는 경우
        else
        {
            DamageToShield = CurrentShield;
            DamageToHealth = BaseDamage - CurrentShield;
        }
    }
    // 보호막이 없는 상태라면 모든 기저 대미지가 체력 차감 수치로 직행합니다
    else
    {
        DamageToShield = 0.f;
        DamageToHealth = BaseDamage;
    }

    // 최종 체력 피해량이 현재 피격자가 들고 있는 잔여 체력보다 클 수 없도록 오버킬 제한을 걸어줍니다
    if (DamageToHealth > CurrentHealth)
    {
        DamageToHealth = CurrentHealth;
    }

    // 최종 연산된 감산 수치들을 피격자의 AttributeSet에 마이너스 반영 값으로 등록합니다
    if (DamageToShield > 0.f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatCapture().ShieldProperty, EGameplayModOp::Additive, -DamageToShield));
    }

    if (DamageToHealth > 0.f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatCapture().HealthProperty, EGameplayModOp::Additive, -DamageToHealth));
    }
}