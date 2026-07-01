// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UFT_AttributeSet::UFT_AttributeSet()
{
}

void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 네트워크 직렬화 전파: 모든 핵심 속성을 데디케이트 서버 환경에서 클라이언트로 유실 없이 동기화하도록 마스터 세팅합니다.
    // REPNOTIFY_Always 규칙을 사용하여 수치가 동일하더라도 패킷 갱신 신호가 오면 상시 리플리케이션 통지를 발생시킵니다.
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

    // 수치 튐 선제 방어선: 내부 수치가 실질적으로 변경되기 직전 가상 함수 게이트를 열어, UI 표현이나 예측 연산에서 비정상적인 최소, 최대 범위를 벗어나지 않도록 클램핑 가둠 제어를 수행합니다.
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

    // 기획 사양 공식 안착: GEEC_Damage 연산기를 통해 가공이 완료된 마스터 대미지 메타 속성이 최종 배달되었을 때 작동하는 실질 체력 감산 파이프라인입니다.
    if (ModifiedAttribute == GetDamageAttribute())
    {
        // 보관소에 가공된 임시 대미지 총량을 기록한 뒤 다음 타격 연산을 위해 원시 대미지 슬롯을 즉시 0으로 비워줍니다.
        const float LocalDamageDone = GetDamage();
        SetDamage(0.0f);

        if (LocalDamageDone > 0.0f)
        {
            // 보호막 선제 차단 연산: 현재 영웅이 보유한 보호막 통장에서 대미지를 먼저 감산합니다.
            const float NewShield = GetShield() - LocalDamageDone;
            if (NewShield >= 0.0f)
            {
                // 보호막 잔여량이 존재한다면 체력 차단 없이 보호막 수치만 갱신 복사합니다.
                SetShield(FMath::Clamp<float>(NewShield, 0.0f, GetMaxShield()));
            }
            else
            {
                // 보호막이 대미지보다 작아 뚫려버린 경우 보호막을 0으로 소멸시키고 남은 음수 피해량을 체력 통장에 더해 최종 차감합니다.
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield;
                SetHealth(FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth()));
            }
        }
    }
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
        // 이동 속도 연동 배관 공사구역
        // 1단계: 속도 수치 오염을 막기 위해 최소 0에서 최대 지정 속도 범위로 데이터를 강제 가둡니다.
        SetMoveSpeed(FMath::Clamp<float>(GetMoveSpeed(), 0.0f, GetMaxMoveSpeed()));
        
        // 2단계: 변경된 속도 수치를 언리얼 순정 물리 무브먼트 컴포넌트의 MaxWalkSpeed 배관에 즉시 동기화 주입합니다.
        if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            // 피격 상태가 된 대상 영웅의 아바타 액터 포인터를 안전하게 견인합니다.
            if (ACharacter* TargetChar = Cast<ACharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get()))
            {
                if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
                {
                    // GAS 속성 지표와 컴포넌트 속도를 1대1 일치시켜 슬로우 디버프나 가속 유틸기가 물리적으로 즉각 공명하도록 연동합니다.
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
}

// 보정 완료: 구버전 헤더 파일과의 1대1 싱크로율을 완벽하게 맞추기 위해 언리얼 매크로와 충돌하는 원시 레퍼런스 참조 기호들을 전량 제거하고 순정 수명 객체로 프리패스 개통을 마쳤습니다.
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