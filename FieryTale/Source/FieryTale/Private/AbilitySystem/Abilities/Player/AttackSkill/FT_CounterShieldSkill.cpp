// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_CounterShieldSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_CounterShieldSkill::UFT_CounterShieldSkill()
    : MovementPenaltyMultiplier(0.6f) // 기획 스펙: 이동 속도 40% 감소 페널티 (원래 속도의 60%로 조정)
    , MaxDuration(4.0f)              // 기획 스펙: 최대 4초 유지
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 우클릭 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 강공격(우클릭) 공용 재사용 대기시간 태그 매핑 (데이터 에셋 설정상 가구야는 6초 쿨타임 작동)
    CooldownTag = FTTags::FTStates::Cooldown_RightClick;

    // 가구야가 반격 가드 태세에 진입했음을 알리는 상태 태그를 부여합니다 (GEEC_Damage에서 이 태그를 읽어 대미지를 무효화합니다)
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.CounterReady")));
}

void UFT_CounterShieldSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

    if (!Character || !Character->GetWeaponData() || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 기획 스펙 반영: 방벽 전개 즉시 시전 중 상태 태그를 루즈 태그로 부여 (다른 스킬 입력 시 취소 인터럽트 연동용)
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

    // 기획 스펙 반영: 방벽 전개 중 가구야 공주의 이동 속도 40% 감산
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        MoveComp->MaxWalkSpeed *= MovementPenaltyMultiplier;
    }

    // 가드 돌입 시점에 무기 데이터 또는 전용 채널링 애니메이션 몽타주가 있다면 비동기 태스크로 가동합니다
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

    // 기획 스펙 반영: 최대 4초 유지 제한 지속 타이머 가동 (4초 도달 시 자동 종료 파이프라인)
    GetWorld()->GetTimerManager().SetTimer(
        BulwarkDurationTimerHandle, 
        FTimerDelegate::CreateUObject(this, &UFT_CounterShieldSkill::EndAbility, Handle, ActorInfo, ActivationInfo, true, false),
        MaxDuration, 
        false
    );
}

void UFT_CounterShieldSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머가 중복 가동되지 않도록 안전하게 제거합니다
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

    // 채널링 루즈 태그 제거 및 상태 해제
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 감소했던 가구야의 이동 속도(40%)를 원래 수치로 다시 안전하게 복구합니다
    if (Character)
    {
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (MovementPenaltyMultiplier > 0.0f)
            {
                MoveComp->MaxWalkSpeed /= MovementPenaltyMultiplier;
            }
        }
    }

    // 가드 태세를 해제하는 시점에 6초 쿨타임을 돌려 무한 방어를 막습니다
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}