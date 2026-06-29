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
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    // 우클릭 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 조준 사격 모드 상태임을 나타내는 태그를 부여합니다
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

    // 정조준 진입에 따라 능력 시스템 컴포넌트를 통해 탄퍼짐 수치를 즉시 최소치로 압축합니다
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        // 저격 모드에서는 완벽한 정밀도를 위해 탄퍼짐 값을 0으로 강제 세팅합니다
        ASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), 0.0f);
    }

    // 스코프 줌인 효과를 위해 카메라 시야각을 60도로 좁힙니다
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
       // 조준을 푸는 즉시 기본 시야각인 90도로 카메라를 복구합니다
       if (CachedCharacter->GetFollowCamera())
       {
          CachedCharacter->GetFollowCamera()->FieldOfView = 90.0f;
       }

       // stopAim 시점에 압축되었던 탄퍼짐 수치를 무기 에셋의 원본 기본 상태로 복원합니다
       if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
       {
           if (UFT_WeaponData* WeaponData = CachedCharacter->GetWeaponData())
           {
               // 에셋에 기획자가 지정해 둔 고유 태생 기본 탄퍼짐 수치를 가져와 채워 넣습니다
               ASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), WeaponData->InitBaseSpread);
           }
       }

       // 연속적인 조준 입력 연타를 제어하기 위해 쿨타임을 정상 가동합니다
       (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}