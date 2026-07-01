// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Calculations/GEEC_Damage.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayTags/FTTags.h"

// [GAS 속성 캡처 구조체] 마스터 속성 집합에서 실시간 대미지 속성을 안전하게 추적 및 스냅샷 백업하기 위한 네이티브 구조체 정의 구역입니다.
struct FT_DamageStatCapture
{
    // [아키텍처 통합] 이 계산기는 복잡한 스탯을 분산 처리하지 않고 오직 Damage 속성 우체통 하나만 집중 타겟팅하여 수치를 가공 배달합니다.
    DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

    FT_DamageStatCapture()
    {
        // 대상의 속성 집합으로부터 대미지 프로퍼티를 런타임에 안전하게 캡처하도록 내부 메모리 주소를 정의합니다.
        DEFINE_ATTRIBUTE_CAPTUREDEF(UFT_AttributeSet, Damage, Target, false);
    }
};

// [멀티스레드 최적화] 스태틱 정적 인스턴스로 관리하여 매 피격 격발 시마다 구조체가 중복 생성되는 연산 릭을 방쇄하고 캡처 메타데이터를 안전하게 유지합니다.
static const FT_DamageStatCapture& DamageStatCapture()
{
    static FT_DamageStatCapture DCapture;
    return DCapture;
}

UGEEC_Damage::UGEEC_Damage()
{
    // 계산기 인스턴스화 단계에서 런타임에 실시간으로 추적 및 가공할 프로퍼티 마스터 장부에 대미지 속성을 확실하게 등록합니다.
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
    
    // =========================================================================
    // [AOS 롤 스타일 팀 컬러 피아식별 가드벽]
    // 멀티플레이어 대전 및 미니언 한타 교전 중 동일 진영 아군끼리의 오사 타격인 경우,
    // 이하의 모든 피해량 연산을 수행하지 않고 조기 종료하여 팀 오사 대미지 면역을 보장합니다.
    // =========================================================================
    if ((SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) ||
        (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)))
    {
        return;
    }
    
    // =========================================================================
    // [특수 기획 사양: 가구야 반격 가드 가동]
    // 영웅 가구야의 반격 가드 태세 버프 태그를 네이티브 계층에서 다이렉트로 필터링합니다.
    // 적 영웅이 방벽을 전개하여 반격 대기 중인 상태라면 모든 타격을 무효화하므로 피해 연산을 전면 소멸시킵니다.
    // =========================================================================
    if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::CounterReady))
    {
        return; 
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // [C-1 버그 박멸 라인 완착]
    // 영웅들의 평타 및 데이터 에셋, 미니언들의 신규 C++ 공격 패턴들이 사출한 
    // 네이티브 대미지 태그 주소지(FTTags::FTCombat::Damage)로부터 기획 수치를 오차 없이 수신합니다.
    float BaseDamage = FMath::Max<float>(Spec.GetSetByCallerMagnitude(FTTags::FTCombat::Damage, false, 0.0f), 0.0f);
    
    // --- 향후 밸런싱 및 계산식 확장 구역 (방어력 감산 연산, 치명타 가산 배율, 증강 카드 가중치 등) ---
    float FinalCalculatedDamage = BaseDamage;
    // -----------------------------------------------------------------------------------------

    // [정석 아키텍처 이관 수술 최종 확인]
    // 체력이나 보호막 중 어떤 장부를 먼저 깎아낼지의 복잡한 실시간 잔여 수명 연산 로직은
    // 이미 원자적 조립을 완료한 마스터 AttributeSet 클래스의 PostGameplayEffectExecute 최종 관문에서 안전하게 통합 처리합니다.
    // 여기서는 최종 가공된 순정 대미지 총량 수치만을 가산 데이터 팩(Additive Modifier)에 실어 속성 집합의 대미지 슬롯으로 무결하게 발송합니다.
    if (FinalCalculatedDamage > 0.0f)
    {
        ExecutionOutputs.AddOutputModifier(FGameplayModifierEvaluatedData(
            DamageStatCapture().DamageProperty, 
            EGameplayModOp::Additive, 
            FinalCalculatedDamage
        ));
    }
}