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

void UFT_AttributeSet::BindDelegates()
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(GetMoveSpeedAttribute()).AddUObject(this, &UFT_AttributeSet::OnMoveSpeedAttributeChanged);
    }
}

void UFT_AttributeSet::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data)
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        if (ACharacter* TargetChar = Cast<ACharacter>(ASC->GetAvatarActor()))
        {
            if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
            {
                MoveComp->MaxWalkSpeed = Data.NewValue;
            }
        }
    }
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
    // 💡 [속도 버프 감옥 해방]: MaxMoveSpeed 한계치에 구속되지 않고 상한 속도를 마음껏 돌파하도록 격리합니다.
    else if (Attribute == GetMoveSpeedAttribute() || Attribute == GetMaxMoveSpeedAttribute())
    {
        NewValue = FMath::Max<float>(NewValue, 0.0f);
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

void UFT_AttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    // 다른 스탯들의 갱신 처리가 필요하다면 이곳에서 수행합니다.
    // (참고: MoveSpeed의 경우 FTPlayerCharacterBase에서 델리게이트를 통해 안전하게 갱신하고 있습니다)
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

            if (ActualDamageApplied > 0.0f && Data.EffectSpec.GetContext().IsValid())
            {
                if (UAbilitySystemComponent* InstigatorASC = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                {
                    bool bIsUltimateDamage = Data.EffectSpec.Def->GetAssetTags().HasTag(FTTags::FTAbilities::UltimateSkill) || 
                                             Data.EffectSpec.DynamicGrantedTags.HasTag(FTTags::FTAbilities::UltimateSkill) ||
                                             InstigatorASC->HasMatchingGameplayTag(FTTags::FTAbilities::UltimateSkill);

                    if (!bIsUltimateDamage)
                    {
                        const UFT_AttributeSet* InstigatorAttributeSet = Cast<UFT_AttributeSet>(InstigatorASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            
                        if (InstigatorAttributeSet)
                        {
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
    // =========================================================================
    // 💡 [완치 구역: PostExecute 내부의 중복 이속 동기화 제거]
    // 가속 상태에서 MoveSpeed를 MaxMoveSpeed로 싹둑 잘라 하향 평준화 시키던 
    // 오래된 Clamp 코드를 소각하고, 순수하게 비정상 연산으로 음수 속도가 되는 현상만 방어합니다.
    // =========================================================================
    else if (ModifiedAttribute == GetMoveSpeedAttribute())
    {
        SetMoveSpeed(FMath::Max<float>(GetMoveSpeed(), 0.0f));
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
                    AController* KillerController = nullptr;
                    if (Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent())
                    {
                        AActor* InstigatorActor = Data.EffectSpec.GetContext().GetInstigatorAbilitySystemComponent()->GetAvatarActor();
                        if (APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
                        {
                            KillerController = InstigatorPawn->GetController();
                        }
                    }

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