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

    if (ModifiedAttribute == GetDamageAttribute())
    {
        const float LocalDamageDone = GetDamage();
        SetDamage(0.0f);

        if (LocalDamageDone > 0.0f)
        {
            float ActualDamageApplied = 0.0f;

            const float NewShield = GetShield() - LocalDamageDone;
            if (NewShield >= 0.0f)
            {
                SetShield(FMath::Clamp<float>(NewShield, 0.0f, GetMaxShield()));
                ActualDamageApplied = LocalDamageDone;
            }
            else
            {
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield;
                
                const float CurrentHealth = GetHealth();
                const float ClampedNewHealth = FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth());
                
                ActualDamageApplied = (CurrentHealth - ClampedNewHealth) + GetShield();
                
                SetHealth(ClampedNewHealth);
            }

            // =========================================================================
            // ⚡ [공격적 획득 선로 - 궁극기 게이지 대미지 비례 충전 파이프라인]
            // =========================================================================
            if (ActualDamageApplied > 0.0f && Data.EffectSpec.GetContext().IsValid())
            {
                if (UAbilitySystemComponent* InstigatorASC = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                {
                    // 💡 [최종 버그 해결선]: 들어온 이펙트 자체에 궁극기 판정 태그가 박혀있는지 정밀 검문합니다.
                    // 프로젝트의 태그 매핑 규칙(FTTags 혹은 공용 계층) 양단을 모두 안전망으로 확인합니다.
                    bool bIsUltimateDamage = Data.EffectSpec.Def->GetAssetTags().HasTag(FGameplayTag::RequestGameplayTag(TEXT("FTTags.FTAbilities.UltimateSkill"))) || 
                                             Data.EffectSpec.Def->GetAssetTags().HasTag(FGameplayTag::RequestGameplayTag(TEXT("FTTags.Abilities.UltimateSkill")));

                    // 💡 궁극기로 가한 대미지가 아닐 때만 게이지를 정직하게 채워줍니다!
                    if (!bIsUltimateDamage)
                    {
                        const UFT_AttributeSet* InstigatorAttributeSet = Cast<UFT_AttributeSet>(InstigatorASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                        
                        if (InstigatorAttributeSet)
                        {
                            // 💡 기획 밸런스 셋업: 대미지 100당 궁극기 2.5% 충전 사양으로 배율을 정상화합니다. (0.5f -> 0.025f)
                            const float UltimateChargeMultiplier = 0.5f;
                            const float UltimateBonus = ActualDamageApplied * UltimateChargeMultiplier;

                            float CurrentSourceUlt = InstigatorAttributeSet->GetUltimateGauge();
                            float NewSourceUlt = FMath::Clamp<float>(CurrentSourceUlt + UltimateBonus, 0.0f, InstigatorAttributeSet->GetMaxUltimateGauge());

                            InstigatorASC->SetNumericAttributeBase(GetUltimateGaugeAttribute(), NewSourceUlt);
                        }
                    }
                }
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