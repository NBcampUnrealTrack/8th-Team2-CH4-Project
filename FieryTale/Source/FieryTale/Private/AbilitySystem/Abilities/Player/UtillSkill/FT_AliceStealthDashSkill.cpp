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
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

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

    // =========================================================================
    // 💡 [완치 구역: 가속 이펙트 컨텍스트 무결성 락인 및 주입 배관]
    // 장부에 시전자(Character) 정보를 완벽하게 결착하여 인가함으로써, 
    // 수선 완료된 FT_AttributeSet의 PostAttributeChange 관문과 100% 치합되도록 보장합니다.
    // =========================================================================
    if (DashSpeedGameplayEffectClass)
    {
        FGameplayEffectSpecHandle SpeedSpecHandle = MakeOutgoingGameplayEffectSpec(DashSpeedGameplayEffectClass, GetAbilityLevel());
        if (SpeedSpecHandle.IsValid() && SpeedSpecHandle.Data.IsValid())
        {
            SpeedSpecHandle.Data->GetContext().AddInstigator(Character, Character);
            SpeedSpecHandle.Data->GetContext().AddSourceObject(Character);

            DashSpeedActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpeedSpecHandle.Data.Get());
        }
    }

    // 캐릭터 및 카메라 크기 축소 보정
    Character->SetCharacterScale(TargetMeshScale);
    Character->SetCameraDistanceScale(TargetMeshScale);

    // =========================================================================
    // 💡 [컴파일 에러 파쇄선 및 순정 GAS 태그 자동화 안착]
    // C++ 내부에서 수동으로 'HasAuthority'나 'AddReplicatedLooseGameplayTag'를 조회하며 
    // 컴파일러를 터트리던 예외 코드를 원천 소각했습니다!
    // 버프 상태 태그(ShrinkActive)는 에디터 GE의 'Target Tags (Granted to Actor)' 채널을 통해 
    // 시스템이 자동으로 넷코드 복제를 처리하도록 안전을 보장합니다.
    // =========================================================================

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
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld() && ShrinkBuffDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
        ShrinkBuffDurationTimerHandle.Invalidate();
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character)
    {
        Character->ResetCharacterScale();
        Character->ResetCameraDistanceScale();
    }

    if (SourceASC)
    {
        // 가속 이펙트 해제 (이펙트가 제거되면 Granted Tag인 ShrinkActive도 순정 GAS 시스템에 의해 자동 제거됩니다)
        if (DashSpeedActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(DashSpeedActiveHandle);
            DashSpeedActiveHandle.Invalidate();
        }

        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);

            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}