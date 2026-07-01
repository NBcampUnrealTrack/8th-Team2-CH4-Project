// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_UltimateGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTags/FTTags.h"

UFT_UltimateGameplayAbility::UFT_UltimateGameplayAbility()
{
    // 궁극기는 고유 인풋 태그 또는 스킬 분류 태그로 관리됩니다.
    // 이 베이스 클래스를 상속받는 알라딘, 앨리스, 가구야, 빨간 망토의 각 궁극기 생성자에서 세부 스펙을 확장합니다.
}

bool UFT_UltimateGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // GAS 순정 시스템 검증: 쿨타임, 침묵, 기절 등 공용 상태 이상 태그에 걸려있다면 시전 승인 라인을 선제 차단합니다.
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
       return false;
    }

    // 기획 반영: 일반 마나 소모 방식이 아닌, 소유자의 AttributeSet을 찾아 현재 궁극기 게이지가 최대치에 달성했는지 정밀 검증합니다.
    if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
       if (const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(ASC->GetAttributeSet(UFT_AttributeSet::StaticClass())))
       {
          float CurrentGauge = AttributeSet->GetUltimateGauge();
          float MaxGauge = AttributeSet->GetMaxUltimateGauge();

          // 게이지가 가득 차지 않았거나 최대 게이지 비정상 설정 시 시전을 원천 차단합니다.
          // 런타임 흐름 요약: 이 조건문이 false를 던지면 UI 단에서도 궁극기 버튼이 비활성화 상태(불이 꺼진 상태)로 안전 연동됩니다.
          if (CurrentGauge < MaxGauge || MaxGauge <= 0.0f)
          {
             return false;
          }
       }
    }

    return true;
}

void UFT_UltimateGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC) return;

    // 섀도잉 예외 해결: 부모 클래스인 UGameplayAbility 순정 멤버 변수인 CostGameplayEffectClass 배관을 직통 연결합니다.
    // 에디터 세팅 가이드: 기획자분들은 각 영웅 궁극기 블루프린트 에셋의 'Cost Gameplay Effect Class' 슬롯에 일회성 자원 소모용 GE 에셋을 지정해야 합니다.
    if (CostGameplayEffectClass)
    {
        // 에디터에서 지정한 일회성 비용 GE의 스펙 핸들을 안전하게 생성합니다.
        FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
        if (CostSpecHandle.IsValid())
         {
            // SetByCaller 시스템을 가동하여 궁극기 게이지를 0.0f으로 강제 청소 초기화하라는 데이터 꼬리표 주소를 붙입니다.
            // 런타임 흐름 요약: 이 스펙이 자가 사출되면 속성 집합(AttributeSet)의 PostGameplayEffectExecute 파이프라인에서 게이지를 즉시 비웁니다.
            CostSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, 0.0f);
            
            // 시전자의 능력 시스템 컴포넌트(ASC)에 직통 사출하여 패킷 동기화를 동시 격발합니다.
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
        }
    }
}