// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : OriginalMaxWalkSpeed(0.0f)    
    , OriginalCapsuleRadius(0.0f)
    , OriginalCapsuleHalfHeight(0.0f)
    , MovementSpeedMultiplier(1.5f)
    , MaxDuration(2.0f)
    , TargetMeshScale(0.5f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::ShrinkActive);
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 마나 자원이 없으므로 현재 15초 시프트 쿨타임 태그가 걸려있는지만 순정 GAS 관문에서 체크합니다.
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 이속 버그 방어선 수술: 런타임 나누기 연산 오차로 기본 속도가 증발하는 릭을 원천 차단하기 위해 가속 전 캐릭터의 순정 최대 걷기 속도 원본을 보관소에 안전하게 선제 백업합니다.
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
        MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * MovementSpeedMultiplier;
    }

    // 기획 스펙 반영: 앨리스의 히트박스 캡슐 콜리전 축소 및 원본 수치 임시 백업 보정 구역입니다.
    if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
    {
        OriginalCapsuleRadius = CapsuleComp->GetUnscaledCapsuleRadius();
        OriginalCapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();

        CapsuleComp->SetCapsuleSize(OriginalCapsuleRadius * TargetMeshScale, OriginalCapsuleHalfHeight * TargetMeshScale, true);
    }
    
    // 시각적 연동: 캐릭터 메시 비주얼도 기획 스펙에 맞추어 절반 크기로 축소 스케일링합니다.
    Character->SetActorScale3D(FVector(TargetMeshScale));

    // 기획 스펙 반영: 무한 축소 치트를 방지하기 위해 정확한 2초 버프 지속 제한 시간 타이머를 가동합니다.
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
    // 2초 지속 시간이 모두 정상 만료되면 표준 종료 마감 관문으로 상태를 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 누수 안전 해제: 능력이 닫히기 시작하면 가동 중이던 2초 지속시간 타이머 핸들을 즉시 청소 수거하고 무효화합니다.
    if (GetWorld() && ShrinkBuffDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
        ShrinkBuffDurationTimerHandle.Invalidate();
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character)
    {
        // 이속 복구 확정 수술: 나눗셈 연산 왜곡 없이 처음에 안전 백업해 두었던 순정 원본 속도 수치 그대로 최대 걷기 속도 배관에 1대1 환원 복구시킵니다.
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (OriginalMaxWalkSpeed > 0.0f)
            {
                MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
            }
        }

        // 물리 크래시 박멸 완료: 백업해 두었던 원본 정수로 캡슐 규격을 고정 초기화하여 지형에 끼이거나 캐릭터가 찌그러지는 현상을 소멸시킵니다.
        if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
        {
            if (OriginalCapsuleRadius > 0.0f && OriginalCapsuleHalfHeight > 0.0f)
            {
                CapsuleComp->SetCapsuleSize(OriginalCapsuleRadius, OriginalCapsuleHalfHeight, true);
            }
        }
        
        // 캐릭터 메시 스케일을 다시 순정 1대1대1 규격으로 안전하게 회귀시킵니다.
        Character->SetActorScale3D(FVector(1.0f));
    }

    // AOS 쿨다운 정책 완착: 신체 축소가 정상 종료되어 원래 크기로 완벽 복구된 바로 이 최종 시점에만 15초 고유 쿨타임을 발동시킵니다.
    // 만약 적의 하드 CC기에 의해 지속시간 도중 강제 취소당한 억까 상황이라면 쿨타임 패널티 없이 리턴 복귀합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}