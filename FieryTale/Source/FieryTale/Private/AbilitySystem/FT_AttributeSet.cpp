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

    // 메타 속성인 Damage가 적용되었을 때 체력 및 보호막 차감을 처리하는 로직입니다.
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
                // 보호막을 초과하는 대미지가 발생했을 때 초과분을 체력(Health)에 적용합니다.
                SetShield(0.0f);
                const float NewHealth = GetHealth() + NewShield;
                
                const float CurrentHealth = GetHealth();
                const float ClampedNewHealth = FMath::Clamp<float>(NewHealth, 0.0f, GetMaxHealth());
                
                ActualDamageApplied = (CurrentHealth - ClampedNewHealth) + GetShield();
                
                SetHealth(ClampedNewHealth);
            }

            // 궁극기 게이지 충전 로직
            if (ActualDamageApplied > 0.0f && Data.EffectSpec.GetContext().IsValid())
            {
                if (UAbilitySystemComponent* InstigatorASC = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                {
                    // 검증 조건:
                    // 1. 대미지 GE 에셋 자체 태그 검증 (AssetTags)
                    // 2. 대미지 스펙의 동적 부여 태그 검증 (DynamicGrantedTags)
                    // 3. 공격자(Instigator)의 ASC에 궁극기 시전 상태 태그가 있는지 확인합니다.
                    bool bIsUltimateDamage = Data.EffectSpec.Def->GetAssetTags().HasTag(FTTags::FTAbilities::UltimateSkill) || 
                                             Data.EffectSpec.DynamicGrantedTags.HasTag(FTTags::FTAbilities::UltimateSkill) ||
                                             InstigatorASC->HasMatchingGameplayTag(FTTags::FTAbilities::UltimateSkill);

                    // 궁극기 대미지가 아닐 때(평타 및 일반 기술) 궁극기 게이지를 누적 충전합니다.
                    if (!bIsUltimateDamage)
                    {
                        const UFT_AttributeSet* InstigatorAttributeSet = Cast<UFT_AttributeSet>(InstigatorASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            
                        if (InstigatorAttributeSet)
                        {
                            // 충전 배율을 적용합니다. (대미지 수치 비례 충전)
                            const float UltimateChargeMultiplier = 0.5f; 
                            const float UltimateBonus = ActualDamageApplied * UltimateChargeMultiplier;

                            float CurrentSourceUlt = InstigatorAttributeSet->GetUltimateGauge();
                            float NewSourceUlt = FMath::Clamp<float>(CurrentSourceUlt + UltimateBonus, 0.0f, InstigatorAttributeSet->GetMaxUltimateGauge());

                            // 공격자의 속성에 클램핑된 새로운 궁극기 게이지 수치를 적용합니다.
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
        // 이동 속도 스탯 변경 시 순정 캐릭터 무브먼트 컴포넌트의 MaxWalkSpeed와 동기화합니다.
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

    // 최종 정산 후 체력이 0에 도달했다면 서버 권한 환경에서 즉각 사망 시퀀스를 처리합니다.
    if (GetHealth() <= 0.0f)
    {
        if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            if (AFTCharacterBase* BaseChar = Cast<AFTCharacterBase>(Data.Target.AbilityActorInfo->AvatarActor.Get()))
            {
                if (BaseChar->HasAuthority())
                {
                    // 대미지 이펙트를 가한 주체(Instigator)의 컨트롤러를 추적합니다.
                    AController* KillerController = nullptr;
                    if (Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                    {
                        AActor* InstigatorActor = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent()->GetAvatarActor();
                        if (APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
                        {
                            KillerController = InstigatorPawn->GetController();
                        }
                    }

                    // 추적한 가해자 컨트롤러를 전달하여 Die 함수를 호출합니다.
                    BaseChar->Die(KillerController);
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