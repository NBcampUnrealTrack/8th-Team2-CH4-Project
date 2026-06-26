#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UFT_AttributeSet::UFT_AttributeSet()
{
}

void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 모든 속성을 유실 없이 데디케이트 서버 환경에서 동기화하도록 셋팅합니다.
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Shield, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, UltimateGauge, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxUltimateGauge, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, WeaponSpread, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxWeaponSpread, COND_None, REPNOTIFY_Always);
}

void UFT_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
}

void UFT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    
    // [기획 핵심 락(Lock) 구현 구역]
    // 이후 여기에 현재 체력이 Max체력을 넘지 못하게 하거나, Shield가 MaxShield를 넘지 못하게 
    // 칼같이 깎아버리는(Clamping) 기획 연산 코드가 입주하게 됩니다.
}

// =============================================================================
// [데디케이트 서버 전용 리플리케이션 알림 콜백 매크로들]
// =============================================================================
void UFT_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Health, OldHealth);
}

void UFT_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxHealth, OldMaxHealth);
}
void UFT_AttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Shield, OldShield);
}

void UFT_AttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxShield, OldMaxShield);
} 

void UFT_AttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MoveSpeed, OldMoveSpeed);
}

void UFT_AttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, AttackPower, OldAttackPower);
}

void UFT_AttributeSet::OnRep_UltimateGauge(const FGameplayAttributeData& OldUltimateGauge)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, UltimateGauge, OldUltimateGauge);
}

void UFT_AttributeSet::OnRep_MaxUltimateGauge(const FGameplayAttributeData& OldMaxUltimateGauge)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxUltimateGauge, OldMaxUltimateGauge);
}

void UFT_AttributeSet::OnRep_WeaponSpread(const FGameplayAttributeData& OldWeaponSpread)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, WeaponSpread, OldWeaponSpread);
}

void UFT_AttributeSet::OnRep_MaxWeaponSpread(const FGameplayAttributeData& OldMaxWeaponSpread)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxWeaponSpread, OldMaxWeaponSpread);
}