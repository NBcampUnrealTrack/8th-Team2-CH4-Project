// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"

#include "AbilitySystemComponent.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : MaxDuration(2.0f)
    , TargetMeshScale(0.5f)
{
    // 인스턴싱 및 네트워크 실행 정책을 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 자산 태그 및 쿨타임 태그를 설정합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 활성화 시 신체 축소 버프 태그를 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::ShrinkActive);
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 조건 및 코스트를 검증합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!Character || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (SkillMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("AliceDashMontage"), SkillMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->ReadyForActivation();
        }
    }

    // 이동 속도 증가 이펙트 적용
    if (DashSpeedGameplayEffectClass)
    {
        FGameplayEffectSpecHandle SpeedSpecHandle = MakeOutgoingGameplayEffectSpec(DashSpeedGameplayEffectClass, GetAbilityLevel());
        if (SpeedSpecHandle.IsValid())
        {
            DashSpeedActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpeedSpecHandle.Data.Get());
        }
    }

    // 캐릭터 크기 축소
    Character->SetCharacterScale(TargetMeshScale);

    // 카메라 거리 축소 보정
    Character->SetCameraDistanceScale(TargetMeshScale);

    // 지속 시간 타이머 설정
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().SetTimer(
            ShrinkBuffDurationTimerHandle,
            this,
            &UFT_AliceStealthDashSkill::OnShrinkBuffFinished,
            MaxDuration,
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AliceStealthDashSkill::OnShrinkBuffFinished()
{
    // 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 해제
    if (GetWorld() && ShrinkBuffDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
        ShrinkBuffDurationTimerHandle.Invalidate();
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character)
    {
        // 캐릭터 크기 원상 복구
        Character->ResetCharacterScale();

        // 카메라 거리 원상 복구
        Character->ResetCameraDistanceScale();
    }

    if (SourceASC)
    {
        // 가속 이펙트 해제
        if (DashSpeedActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(DashSpeedActiveHandle);
            DashSpeedActiveHandle.Invalidate();
        }

        // 어빌리티가 취소되었을 경우 쿨타임 초기화
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);

            // 해당 쿨타임 태그를 가진 이펙트를 제거합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}