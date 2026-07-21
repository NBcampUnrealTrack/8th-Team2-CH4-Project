// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/FTPlayerCharacterBase.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)
{
    // 인스턴싱 및 네트워크 실행 정책을 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 자산 태그 및 쿨타임 태그를 설정합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 활성화 시 비행 버프 태그를 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Flying);
}

void UFT_AladdinFlySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 쿨타임 확인
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 코스트 및 쿨타임 적용
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!Character || !MoveComp || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("FlyMontage"), LoadedMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->ReadyForActivation();
        }
    }

    // 앨리스 유틸과 동일한 속도 증가 이펙트 적용
    AFTPlayerCharacterBase* PlayerCharacter = Cast<AFTPlayerCharacterBase>(Character);
    
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
    else
    {
        // 이펙트 클래스가 설정되지 않은 경우를 위한 폴백 (이동 속도 1.5배)
        ApplyMovementPenalty(1.5f);
    }
    
    // FOV 펀치 효과로 시각적인 속도감 추가
    if (PlayerCharacter)
    {
        PlayerCharacter->PlayUtilSkillFovPunch();
    }
    
    // 지속 시간 타이머 설정
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            FlyDurationTimerHandle, 
            this, 
            &UFT_AladdinFlySkill::OnFlyDurationExpired, 
            FlyDuration, 
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AladdinFlySkill::OnFlyDurationExpired()
{
    // 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinFlySkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 해제
    if (GetWorld() && FlyDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(FlyDurationTimerHandle);
        FlyDurationTimerHandle.Invalidate();
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character && Character->GetCharacterMovement())
    {
        // 걷기 모드로 복귀 방어 코드 (혹시 모를 상황 대비)
        if (Character->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
        {
            Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
        }
    }

    if (SourceASC)
    {
        // 가속 이펙트 해제 (이펙트 클래스가 설정된 경우)
        if (DashSpeedActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(DashSpeedActiveHandle);
            DashSpeedActiveHandle.Invalidate();
        }
        else
        {
            // 폴백으로 사용된 속도 증가 버프 해제
            RemoveMovementPenalty();
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}