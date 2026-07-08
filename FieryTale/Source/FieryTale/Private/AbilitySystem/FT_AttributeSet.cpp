// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/FTCharacterBase.h"

UFT_AttributeSet::UFT_AttributeSet()
{
}

void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // [네트워크 직렬화 전파 장부]
    // 모든 핵심 속성을 데디케이트 서버 환경에서 클라이언트로 유실 없이 동기화하도록 세팅합니다.
    // COND_None과 REPNOTIFY_Always 조합으로 수치 변경 시 무조건 원격 클라이언트에 즉시 전송 및 RepNotify 콜백을 강제 격발합니다.
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Shield, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxMoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, UltimateGauge, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxUltimateGauge, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, WeaponSpread, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxWeaponSpread, COND_None, REPNOTIFY_Always);
}

void UFT_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // [수치 튐 선제 방어선]
    // 속성의 현재 값(Current Value)이 실질적으로 변경되기 직전에 호출되는 시스템 게이트입니다.
    // UI 나 기타 인라인 수정 시 수치가 정상 범위(0 ~ Max)를 이탈하지 않도록 클램핑 가둠 제어를 수행합니다.
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp<float>(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetShieldAttribute())
    {
        NewValue = FMath::Clamp<float>(NewValue, 0.0f, GetMaxShield());
    }
    else if (Attribute == GetMoveSpeedAttribute())
    {
        NewValue = FMath::Clamp<float>(NewValue, 0.0f, GetMaxMoveSpeed());
    }
    else if (Attribute == GetUltimateGaugeAttribute())
    {
        NewValue = FMath::Clamp<float>(NewValue, 0.0f, GetMaxUltimateGauge());
    }
    else if (Attribute == GetWeaponSpreadAttribute())
    {
        NewValue = FMath::Clamp<float>(NewValue, 0.0f, GetMaxWeaponSpread());
    }
}

void UFT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    
    const FGameplayAttribute ModifiedAttribute = Data.EvaluatedData.Attribute;

    // [기획 사양 공식 대미지 파이프라인]
    if (ModifiedAttribute == GetDamageAttribute())
    {
        const float LocalDamageDone = GetDamage();
        SetDamage(0.0f);

        if (LocalDamageDone > 0.0f)
        {
            // [실제 가해진 최종 피해량 실측 변수] 보호막과 체력 연산 후 최종 가산 수치로 활용됩니다.
            float ActualDamageApplied = 0.0f;

            // [보호막 선제 차단 연산]
            const float NewShield = GetShield() - LocalDamageDone;
            if (NewShield >= 0.0f)
            {
                SetShield(FMath::Clamp<float>(NewShield, 0.0f, GetMaxShield()));
                // 보호막만 깠더라도 상대에게 유효타를 입혔으므로 대미지 전량 실측
                ActualDamageApplied = LocalDamageDone;
            }
            else
            {
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield; // NewShield가 음수이므로 감산
                
                // 💡 [실 대미지 정밀 계산릭 보정]: 체력이 오버킬(음수)되는 수치를 제외하고, 
                // 타깃의 현재 체력통 한도 내에서 실제로 깎인 실질 대미지만 발라냅니다.
                const float CurrentHealth = GetHealth();
                const float ClampedNewHealth = FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth());
                
                // 실제 깎인 체력량 + 쉴드가 버텨준 수치 = 진짜 가해진 피해
                ActualDamageApplied = (CurrentHealth - ClampedNewHealth) + GetShield();
                
                SetHealth(ClampedNewHealth);
            }

            // =========================================================================
            // ⚡ [공격적 획득 선로 - 궁극기 게이지 대미지 비례 충전 파이프라인]
            // 피해를 입은 타깃이 아닌, '피해를 입힌 시전자(Instigator)'의 ASC를 역추적합니다.
            // =========================================================================
            if (ActualDamageApplied > 0.0f && Data.EffectSpec.GetContext().IsValid())
            {
                // 타격을 가한 가해자(시전자)의 어빌리티 시스템 컴포넌트를 호출합니다.
                if (UAbilitySystemComponent* InstigatorASC = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                {
                    // 시전자의 스탯 장부(AttributeSet)를 색출합니다.
                    const UFT_AttributeSet* InstigatorAttributeSet = Cast<UFT_AttributeSet>(InstigatorASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                    
                    if (InstigatorAttributeSet)
                    {
                        // 기획 밸런스 셋업: 대미지 100당 궁극기 2.5% 충전 사양 (0.025f 계수)
                        // 추후 전역 변수나 DataAsset으로 승수를 분리 빼내면 밸런싱이 더 편해집니다.
                        const float UltimateChargeMultiplier = 0.025f;
                        const float UltimateBonus = ActualDamageApplied * UltimateChargeMultiplier;

                        // 시전자의 현재 궁극기 잔고를 확인 후 보너스를 합산 주입합니다.
                        float CurrentSourceUlt = InstigatorAttributeSet->GetUltimateGauge();
                        float NewSourceUlt = FMath::Clamp<float>(CurrentSourceUlt + UltimateBonus, 0.0f, InstigatorAttributeSet->GetMaxUltimateGauge());

                        // [서버 강제 동기화 락인]: 시전자의 궁극기 속성 수치를 정석대로 강제 업데이트 갱신합니다.
                        // 서버 권한에서 셋 수치가 변경되는 순간, 상단에 선언해 둔 DOREPLIFETIME_CONDITION_NOTIFY에 의해
                        // 가해자 화면(UI 등)으로 실시간 패킷 복제 전파가 격발됩니다.
                        InstigatorASC->SetNumericAttributeBase(GetUltimateGaugeAttribute(), NewSourceUlt);
                    }
                }
            }
        }
    }
    // [실질 수치 변경 후 최종 안전 클램핑 구역]
    else if (ModifiedAttribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp<float>(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (ModifiedAttribute == GetShieldAttribute())
    {
        SetShield(FMath::Clamp<float>(GetShield(), 0.0f, GetMaxShield()));
    }
    else if (ModifiedAttribute == GetMoveSpeedAttribute())
    {
        SetMoveSpeed(FMath::Clamp<float>(GetMoveSpeed(), 0.0f, GetMaxMoveSpeed()));
        if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            if (ACharacter* TargetChar = Cast<ACharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get()))
            {
                if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
                {
                    MoveComp->MaxWalkSpeed = GetMoveSpeed();
                }
            }
        }
    }
    else if (ModifiedAttribute == GetUltimateGaugeAttribute())
    {
        SetUltimateGauge(FMath::Clamp<float>(GetUltimateGauge(), 0.0f, GetMaxUltimateGauge()));
    }
    else if (ModifiedAttribute == GetWeaponSpreadAttribute())
    {
        SetWeaponSpread(FMath::Clamp<float>(GetWeaponSpread(), 0.0f, GetMaxWeaponSpread()));
    }

    // [정석 마스터 죽음 파이프라인]
    if (GetHealth() <= 0.0f)
    {
        if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            if (AFTCharacterBase* BaseChar = Cast<AFTCharacterBase>(Data.Target.AbilityActorInfo->AvatarActor.Get()))
            {
                if (BaseChar->HasAuthority())
                {
                    BaseChar->Die();
                }
            }
        }
    }
}

// =========================================================================
// [클라이언트 네트워크 복제 수신부 (RepNotify)]
// 데디케이트 서버가 전송한 압축 스탯 패킷이 클라이언트에 도착하는 순간 
// GAS 코어 예측 장부에 원본 값을 기록하는 순정 매크로 파이프라인들입니다.
// =========================================================================

void UFT_AttributeSet::OnRep_Health(const FGameplayAttributeData OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Health, OldHealth);
}

void UFT_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxHealth, OldMaxHealth);
}

void UFT_AttributeSet::OnRep_Shield(const FGameplayAttributeData OldShield)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Shield, OldShield);
}

void UFT_AttributeSet::OnRep_MaxShield(const FGameplayAttributeData OldMaxShield)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxShield, OldMaxShield);
} 

void UFT_AttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData OldMoveSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MoveSpeed, OldMoveSpeed);
}

void UFT_AttributeSet::OnRep_MaxMoveSpeed(const FGameplayAttributeData OldMaxMoveSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxMoveSpeed, OldMaxMoveSpeed);
}

void UFT_AttributeSet::OnRep_AttackPower(const FGameplayAttributeData OldAttackPower)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, AttackPower, OldAttackPower);
}

void UFT_AttributeSet::OnRep_UltimateGauge(const FGameplayAttributeData OldUltimateGauge)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, UltimateGauge, OldUltimateGauge);
}

void UFT_AttributeSet::OnRep_MaxUltimateGauge(const FGameplayAttributeData OldMaxUltimateGauge)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxUltimateGauge, OldMaxUltimateGauge);
}

void UFT_AttributeSet::OnRep_WeaponSpread(const FGameplayAttributeData OldWeaponSpread)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, WeaponSpread, OldWeaponSpread);
}

void UFT_AttributeSet::OnRep_MaxWeaponSpread(const FGameplayAttributeData OldMaxWeaponSpread)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxWeaponSpread, OldMaxWeaponSpread);
}