// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Calculations/GEEC_Damage.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayTags/FTTags.h"

// [순정 GAS 속성 캡처 구조체 정밀 수선]
// 런타임 스냅샷 백업 및 멀티스레드 환경에서 메모리 왜곡 릭을 방지하기 위해 
// 속성 정의 장부와 캡처 정의를 정석대로 구조체 내부에 완착시킵니다.
struct FT_DamageStatCapture
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Damage); // DamageDef와 DamageProperty를 자동 생성하는 순정 매크로

    FT_DamageStatCapture()
    {
        // 시전자가 타깃에게 가한 대미지 속성을 실시간 캡처하기 위해 메타데이터 주소를 연동합니다.
        // 스냅샷(bSnapshot)을 false로 두어 실시간 계산식의 무결성을 유지합니다.
        DEFINE_ATTRIBUTE_CAPTUREDEF(UFT_AttributeSet, Damage, Target, false);
    }
};

// [멀티스레드 최적화] 싱글톤 정적 인스턴스로 관리하여 연산 부하를 제로로 수렴시킵니다.
static const FT_DamageStatCapture& DamageStatCapture()
{
    static FT_DamageStatCapture DCapture;
    return DCapture;
}

UGEEC_Damage::UGEEC_Damage()
{
    // 계산기가 가동될 때 실시간으로 추적 및 가공할 프로퍼티 마스터 장부에 대미지 속성을 확실하게 가산 등록합니다.
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
    
    // [AOS 롤 스타일 팀 컬러 피아식별 가드벽] 아군간의 팀킬 피해 연산을 원천 차단합니다.
    if ((SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) ||
        (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)))
    {
        return;
    }
    
    // [특수 기획 사양: 가구야 반격 가드 가동] 
    // 적 영웅 가구야가 반격 가드 태세 버프 태그(CounterReady) 상태라면 대미지 연산을 즉시 소멸 무효화합니다.
    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::CounterReady))
    {
        return; 
    }

    // [특수 기획 사양: 레드후드 구르기 회피 가동]
    // 적이 구르기(Evading) 중이라면 모든 대미지 판정을 완벽히 흡수(무시)합니다. (회피 무적 I-Frame)
    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::Evading))
    {
        return;
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    
    // 영웅들의 평타 및 데이터 에셋, 미니언들의 공격 패턴들이 사출한 네이티브 대미지 태그 주소지로부터 기획 수치를 오차 없이 수신합니다.
    float BaseDamage = FMath::Max<float>(Spec.GetSetByCallerMagnitude(FTTags::FTCombat::Damage, false, 0.0f), 0.0f);
    
    // --- 향후 밸런싱 및 계산식 확장 구역 (방어력 감산 연산, 치명타 가산 배율, 증강 카드 가중치 등) ---
    float FinalCalculatedDamage = BaseDamage;

    // [정석 아키텍처 이관 수술 최종 확인]
    // 댕글링 크래시를 유발하는 DamageProperty 포인터를 소각하고,
    // 매크로 내부에서 무결하게 추출되는 스탯 고유 속성 결정자(DamageDef.AttributeToCapture)를 아웃풋 통신망에 탑재합니다.
    if (FinalCalculatedDamage > 0.0f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(
            DamageStatCapture().DamageDef.AttributeToCapture, // 구조적 메모리 릭 완치선
            EGameplayModOp::Additive, 
            FinalCalculatedDamage
        ));
    }
}