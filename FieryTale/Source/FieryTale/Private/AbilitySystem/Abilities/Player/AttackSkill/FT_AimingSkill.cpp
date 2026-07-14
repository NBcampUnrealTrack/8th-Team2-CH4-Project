// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AimingSkill.h"
#include "Camera/CameraComponent.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_AimingSkill::UFT_AimingSkill()
{
    // 인스턴싱 및 네트워크 실행 정책을 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    // 어빌리티 자산 태그를 설정합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 어빌리티 활성화 시 조준 상태 태그를 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTCombat::WeaponMode_Aiming);
}

void UFT_AimingSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CachedCharacter = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!CachedCharacter)
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
       return;
    }

    // 조준 시 탄퍼짐을 최소화합니다.
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        ASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), 0.0f);
    }

    // 카메라 시야각을 좁힙니다.
    if (CachedCharacter->GetFollowCamera())
    {
       CachedCharacter->GetFollowCamera()->FieldOfView = 60.0f;
    }
}

void UFT_AimingSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (CachedCharacter)
    {
       // 조준 종료 시 카메라 시야각을 원래대로 복구합니다.
       if (CachedCharacter->GetFollowCamera())
       {
          CachedCharacter->GetFollowCamera()->FieldOfView = 90.0f;
       }

       // 탄퍼짐 수치를 원래대로 복구합니다.
       if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
       {
           if (UFT_WeaponData* WeaponData = CachedCharacter->GetWeaponData())
           {
               ASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), WeaponData->InitBaseSpread);
           }
       }

       // 조준 종료 시 쿨타임을 적용합니다.
       (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}