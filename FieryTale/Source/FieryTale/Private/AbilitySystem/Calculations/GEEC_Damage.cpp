// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Calculations/GEEC_Damage.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayTags/FTTags.h"

// 마스터 속성 집합에서 실시간 대미지 속성을 안전하게 캡처하기 위한 GAS 내부 구조체 정의 구역입니다.
struct FT_DamageStatCapture
{
    // 아키텍처 통합: 이 계산기는 속성 집합의 복잡한 스탯을 여러 개 건드리지 않고 오직 Damage 속성 우체통 하나만 집중 타겟팅하여 수치를 가공 배달합니다.
    DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

    FT_DamageStatCapture()
    {
        // 대상의 속성 집합으로부터 대미지 프로퍼티를 런타임에 안전하게 캡처하도록 내부 배관 주소를 정의합니다.
        DEFINE_ATTRIBUTE_CAPTUREDEF(UFT_AttributeSet, Damage, Target, false);
    }
};

// 멀티스레드 환경 및 연산 성능 최적화를 위해 단일 정적 스태틱 인스턴스로 안전하게 캡처 메타데이터를 유지합니다.
static const FT_DamageStatCapture& DamageStatCapture()
{
    static FT_DamageStatCapture DCapture;
    return DCapture;
}

UGEEC_Damage::UGEEC_Damage()
{
    // 계산기 객체 생성 시 런타임에 실시간으로 추적 및 가공할 프로퍼티 목록에 대미지 속성을 마스터 락인합니다.
    RelevantAttributesToCapture.Add(DamageStatCapture().DamageDef);
}

void UGEEC_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& ExecutionOutputs) const
{
    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
   
    if (!SourceASC || !TargetASC)
    {
        return;
    }
    
    // 보정 완료: 문자열 기반 하드코딩 유실 태그를 완벽 철거하고 TTags 매니저 허브의 네이티브 진영 태그를 직통 대조합니다.
    // 멀티플레이어 대전 한타 교전 중 동일 진영 아군끼리의 오사 타격인 경우, 이하의 피해량 연산을 일체 수행하지 않고 즉시 조기 종료합니다.
    if ((SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) ||
        (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)))
    {
        return;
    }
    
    // 보정 완료: 가구야 공주의 반격 가드 태세 버프 태그를 네이티브 계층 상수로 필터링합니다.
    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::CounterReady))
    {
        // 기획 사양 공식 안착: 적 영웅이 방벽을 전개하여 반격 가동 대기 중인 상태라면 모든 타격을 전면 무효화하므로 대미지 수치를 런타임에 소멸시킵니다.
        return; 
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // 버그 방어선 완착: 4인방의 모든 평타 및 무기 데이터 에셋, 차징 샷, 관통 은탄 스킬들이 사출한 네이티브 대미지 주소지로부터 기획 수치를 안전하게 추출합니다.
    float BaseDamage = FMath::Max<float>(Spec.GetSetByCallerMagnitude(FTTags::FTCombat::Damage, false, 0.0f), 0.0f);
    
    // --- 향후 밸런싱 및 계산식 확장 구역 (방어력 감산 연산, 치명타 가산 배율, 증강 카드 가중치 등) ---
    float FinalCalculatedDamage = BaseDamage;
    
    // -----------------------------------------------------------------

    // 정석 아키텍처 이관 수술 완료: 체력이나 보호막 중 어떤 통장을 먼저 감산할지의 복잡한 잔여 수명 연산 로직은
    // 이미 원자적 조립을 완료한 마스터 FT_AttributeSet 클래스의 PostGameplayEffectExecute 최종 게이트웨이 관문에서 안전하게 통합 전담 처리합니다.
    // 여기서는 최종 가공된 순정 대미지 총량 수치만을 가산 데이터 팩(Additive Modifier)에 실어 속성 집합의 대미지 슬롯으로 무결하게 발송 배달합니다.
    if (FinalCalculatedDamage > 0.0f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(
            DamageStatCapture().DamageProperty, 
            EGameplayModOp::Additive, 
            FinalCalculatedDamage
        ));
    }
}