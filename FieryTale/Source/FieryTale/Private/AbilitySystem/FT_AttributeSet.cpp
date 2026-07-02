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
    // 계산기(ExecutionCalculation)를 통해 가공이 완료된 Meta Attribute인 Damage 가 배달되었을 때 처리하는 구역입니다.
    if (ModifiedAttribute == GetDamageAttribute())
    {
        // 배달된 대미지 수치를 로컬 스택에 백업한 뒤 즉시 0으로 초기화하여 다음 타격을 위해 대미지 우체통을 비웁니다.
        const float LocalDamageDone = GetDamage();
        SetDamage(0.0f);

        if (LocalDamageDone > 0.0f)
        {
            // [보호막 선제 차단 연산] 현재 영웅이 보유한 보호막(Shield) 통장에서 대미지를 먼저 차감합니다.
            const float NewShield = GetShield() - LocalDamageDone;
            if (NewShield >= 0.0f)
            {
                // 보호막 잔여량이 남았다면 쉴드만 깎고 체력은 온전히 보존합니다.
                SetShield(FMath::Clamp<float>(NewShield, 0.0f, GetMaxShield()));
            }
            else
            {
                // 보호막이 통째로 파괴되었다면, 보호막을 0으로 털어버리고 남은 관통 대미지를 체력(Health)에서 차감합니다.
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield; // NewShield 가 음수이므로 더하면 감산됩니다.
                SetHealth(FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth()));
            }
        }
    }
    // [실질 수치 변경 후 최종 안전 클램핑 구역]
    // GameplayEffect에 의해 베이스 값(Base Value)이 바뀐 후 최종 상태 장부를 안전 범위로 다시 가둡니다.
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
        // 이동 속도 속성이 이펙트(둔화 버프 등)에 의해 변경된 경우 최종 클램핑을 수행합니다.
        SetMoveSpeed(FMath::Clamp<float>(GetMoveSpeed(), 0.0f, GetMaxMoveSpeed()));
        
        // [물리 이속 컴포넌트 강제 동기화]
        // GAS 속성만 바뀌고 실제 캐릭터가 느려지지 않는 현상을 막기 위해 CharacterMovement 의 MaxWalkSpeed 주입선까지 다이렉트로 개통합니다.
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
    // 실시간 피해 누적으로 인해 체력 속성이 최종적으로 0f 이하에 도달했는지 정밀 감지선을 구동합니다.
    if (GetHealth() <= 0.0f)
    {
        if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            // 피격 상태가 된 아바타 액터를 마스터 베이스 캐릭터 클래스로 캐스팅 견인합니다.
            if (AFTCharacterBase* BaseChar = Cast<AFTCharacterBase>(Data.Target.AbilityActorInfo->AvatarActor.Get()))
            {
                // 서버 권한에서만 사망 진입 — Die()가 Dead 태그 부여, 입력/이동 차단,
                // 사망 델리게이트 브로드캐스트(컨트롤러의 리스폰 예약)까지 처리한다.
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