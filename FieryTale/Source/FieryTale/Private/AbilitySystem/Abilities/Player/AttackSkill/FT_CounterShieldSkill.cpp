// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_CounterShieldSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_CounterShieldSkill::UFT_CounterShieldSkill()
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 우클릭 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 가구야가 반격 가드 태세에 진입했음을 알리는 상태 태그를 부여합니다 (GEEC_Damage에서 이 태그를 읽어 대미지를 무효화합니다)
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.CounterReady")));
}

void UFT_CounterShieldSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!Character || !Character->GetWeaponData())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 가드 돌입 시점에 무기 데이터 또는 전용 채널링 애니메이션 몽타주가 있다면 비동기 태스크로 가동합니다
    // 추후 가드 유지용 루프 애니메이션이나 진입 방어 몽타주를 무기 데이터에 추가하여 연동할 수 있습니다
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    if (WeaponData && WeaponData->AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), WeaponData->AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 애니메이션이 자연스럽게 끝나거나 다른 제어 명령으로 취소되었을 때 가드를 풀도록 예약합니다
            MontageTask->OnCompleted.AddDynamic(this, &UFT_CounterShieldSkill::K2_EndAbility);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_CounterShieldSkill::K2_EndAbility);
            MontageTask->ReadyForActivation();
        }
    }
    
    // 추후 구현 포인트 가드 활성화 상태 진입 시 클라이언트 화면에 배치할 전용 방어 시각 이펙트 및 가드 적중 사운드 부착 구역
}

void UFT_CounterShieldSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 가드 태세를 해제하는 시점에 쿨타임을 돌려 무한 방어를 막습니다
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }
    
    // 추후 구현 포인트 가드 해제 시점에 재생할 잔여 이펙트 제거 및 후속 상태 변환 연산 구역
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}