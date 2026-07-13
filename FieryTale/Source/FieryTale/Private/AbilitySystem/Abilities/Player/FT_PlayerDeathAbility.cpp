// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/FT_PlayerDeathAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/FTCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "Animation/AnimMontage.h"

UFT_PlayerDeathAbility::UFT_PlayerDeathAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

    FGameplayTagContainer AssetTagContainer;
    SetAssetTags(AssetTagContainer);

    // FTTags::Events::CharacterDeath 게임플레이 이벤트가 들어오면 자동으로 활성화되도록 트리거 등록
    FAbilityTriggerData TriggerData;
    TriggerData.TriggerTag = FTTags::Events::CharacterDeath;
    TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(TriggerData);
}

void UFT_PlayerDeathAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AFTCharacterBase* Character = ActorInfo ? Cast<AFTCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAnimMontage* MontageToPlay = Character ? Character->GetDeathMontage() : nullptr;

    if (!Character || !MontageToPlay)
    {
        if (Character)
        {
            Character->FinishDying();
        }
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilityTask_PlayMontageAndWait* DeathMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        TEXT("DeathMontageTask"),
        MontageToPlay,
        1.0f,
        NAME_None,
        false
    );

    if (DeathMontageTask)
    {
        DeathMontageTask->OnCompleted.AddDynamic(this, &UFT_PlayerDeathAbility::OnDeathMontageCompletedOrInterrupted);
        DeathMontageTask->OnBlendOut.AddDynamic(this, &UFT_PlayerDeathAbility::OnDeathMontageCompletedOrInterrupted);
        DeathMontageTask->OnInterrupted.AddDynamic(this, &UFT_PlayerDeathAbility::OnDeathMontageCompletedOrInterrupted);
        DeathMontageTask->OnCancelled.AddDynamic(this, &UFT_PlayerDeathAbility::OnDeathMontageCompletedOrInterrupted);

        DeathMontageTask->ReadyForActivation();
    }
    else
    {
        Character->FinishDying();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UFT_PlayerDeathAbility::OnDeathMontageCompletedOrInterrupted()
{
    if (AFTCharacterBase* Character = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo()))
    {
        Character->FinishDying();
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
