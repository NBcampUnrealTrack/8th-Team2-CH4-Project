// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"

UFT_AttributeSet::UFT_AttributeSet()
{
}

void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 네트워크 환경에서 모든 속성 데이터가 안정적으로 동기화되도록 복제 조건을 확정합니다.
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

    // 스탯이 실시간으로 변경되기 직전 최솟값과 최댓값 범위 내부로 정밀 클램핑을 수행합니다.
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

    // 메타 속성인 Damage가 주입되었을 때 실질적인 체력 및 보호막 차감 연산을 전개하는 정산소 분기입니다.
    if (ModifiedAttribute == GetDamageAttribute())
    {
        const float LocalDamageDone = GetDamage();
        SetDamage(0.0f);

        if (LocalDamageDone > 0.0f)
        {
            float ActualDamageApplied = 0.0f;

            // 1단계 연산: 보호막(Shield)을 먼저 깎아 대미지를 흡수합니다.
            const float NewShield = GetShield() - LocalDamageDone;
            if (NewShield >= 0.0f)
            {
                SetShield(FMath::Clamp<float>(NewShield, 0.0f, GetMaxShield()));
                ActualDamageApplied = LocalDamageDone;
            }
            else
            {
                // 보호막을 초과하는 대미지가 발생했을 때 잔여 화력을 체력(Health)으로 이관합니다.
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield;
                
                const float CurrentHealth = GetHealth();
                const float ClampedNewHealth = FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth());
                
                ActualDamageApplied = (CurrentHealth - ClampedNewHealth) + GetShield();
                
                SetHealth(ClampedNewHealth);
            }

            // 공격적 획득 선로: 궁극기 게이지 대미지 비례 충전 파이프라인
            if (ActualDamageApplied > 0.0f && Data.EffectSpec.GetContext().IsValid())
            {
                if (UAbilitySystemComponent* InstigatorASC = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                {
                    // 3중 무결성 크로스 가드선 타설:
                    // 1. 대미지 GE 에셋 자체 태그 검문 (AssetTags)
                    // 2. 대미지 스펙의 동적 부여 태그 검문 (DynamicGrantedTags)
                    // 3. 공격자(Instigator)의 ASC 몸뚱이에 궁극기 시전 상태 태그가 살아있는지 직접 검문합니다.
                    bool bIsUltimateDamage = Data.EffectSpec.Def->GetAssetTags().HasTag(FTTags::FTAbilities::UltimateSkill) || 
                                             Data.EffectSpec.DynamicGrantedTags.HasTag(FTTags::FTAbilities::UltimateSkill) ||
                                             InstigatorASC->HasMatchingGameplayTag(FTTags::FTAbilities::UltimateSkill);

                    // 궁극기로 가한 대미지 피드백 루프가 확실히 아닐 때(평타 및 일반 기술)만 다음 궁극기 게이지를 누적 충전합니다.
                    if (!bIsUltimateDamage)
                    {
                        const UFT_AttributeSet* InstigatorAttributeSet = Cast<UFT_AttributeSet>(InstigatorASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            
                        if (InstigatorAttributeSet)
                        {
                            // 기획 밸런스 셋업 배율을 반영합니다. (대미지 수치 비례 충전 사양)
                            const float UltimateChargeMultiplier = 0.5f; 
                            const float UltimateBonus = ActualDamageApplied * UltimateChargeMultiplier;

                            float CurrentSourceUlt = InstigatorAttributeSet->GetUltimateGauge();
                            float NewSourceUlt = FMath::Clamp<float>(CurrentSourceUlt + UltimateBonus, 0.0f, InstigatorAttributeSet->GetMaxUltimateGauge());

                            // 스탯 소유주(공격자)의 오리지널 스탯 장부에 클램핑된 신규 게이지 수치를 주입합니다.
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
        // 이동 속도 스탯 변경 시 순정 캐릭터 무브먼트 컴포넌트의 MaxWalkSpeed와 동기화하여 연산 괴리를 소각합니다.
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

    // 최종 정산 후 체력이 0에 도달했다면 오너쉽 권한이 있는 서버 분기에서 즉각 사망 시퀀스를 처리합니다.
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