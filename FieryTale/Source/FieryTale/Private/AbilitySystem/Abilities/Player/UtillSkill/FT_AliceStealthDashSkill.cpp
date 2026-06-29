// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameplayTags/FTTags.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : DashSpeed(2000.0f)
    , DashDuration(0.25f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // 은신 상태 태그를 부여하여 기동과 동시에 정보를 차단합니다
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.Stealth")));
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

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character) return;

    FVector DashDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
    if (DashDirection.IsNearlyZero()) DashDirection = Character->GetActorForwardVector();

    UAbilityTask_ApplyRootMotionConstantForce* DashTask = 
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this, FName("AliceDash"), DashDirection, DashSpeed, DashDuration, 
            false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f, false);

    if (DashTask)
    {
        DashTask->OnFinish.AddDynamic(this, &UFT_AliceStealthDashSkill::OnDashFinished);
        DashTask->ReadyForActivation();
    }
}

void UFT_AliceStealthDashSkill::OnDashFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!bWasCancelled) CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}