// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : MovementSpeedMultiplier(1.5f) // 기획 스펙: 이동 속도 50% 증가
    , MaxDuration(2.0f)             // 기획 스펙: 2초간 버프 유지
    , TargetMeshScale(0.5f)         // 히트박스 축소를 위한 목표 스케일 (절반 크기)
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Shift 유틸기 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // Shift 이동/생존 기술 공용 재사용 대기시간 태그 매핑 (15초 쿨타임 작동)
    CooldownTag = FTTags::FTStates::Cooldown_UtilSkill;

    // 버프 유지 시간 동안 캐릭터의 상태를 나타낼 수 있는 태그 지정 구역
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.ShrinkActive")));
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 스킬을 쓸 수 있는 자원 상태를 검사하고 승인 처리합니다
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 1. 기획 스펙 반영: 캐릭터 이동 속도 50% 즉시 가속
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        MoveComp->MaxWalkSpeed *= MovementSpeedMultiplier;
    }

    // 2. 기획 스펙 반영: 앨리스의 히트박스(캡슐 콜리전) 및 캐릭터 크기 축소 처리
    if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
    {
        // 원래 캡슐 크기를 기억해두거나 비율 기반으로 절반 축소
        CapsuleComp->SetCapsuleRadius(CapsuleComp->GetUnscaledCapsuleRadius() * TargetMeshScale);
        CapsuleComp->SetCapsuleHalfHeight(CapsuleComp->GetUnscaledCapsuleHalfHeight() * TargetMeshScale);
    }
    Character->SetActorScale3D(FVector(TargetMeshScale));

    // 3. 기획 스펙 반영: 2초 정확한 버프 지속 타이머 가동
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

    // 추후 구현 포인트: 몸이 작아지는 연기와 펑 소리를 낼 클라이언트 나이아가라 이펙트 및 사운드 구역
}

void UFT_AliceStealthDashSkill::OnShrinkBuffFinished()
{
    // 2초 지속 시간이 다 되면 어빌리티를 마감하고 상태 복구 관문으로 진입합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 가동 중인 지속 타이머를 안전하게 클리어합니다
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        // 4. 원래 속도로 안전 복구
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (MovementSpeedMultiplier > 0.0f)
            {
                MoveComp->MaxWalkSpeed /= MovementSpeedMultiplier;
            }
        }

        // 5. 원래 히트박스 크기 및 메쉬 스케일 복구
        if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
        {
            CapsuleComp->SetCapsuleRadius(CapsuleComp->GetUnscaledCapsuleRadius() / TargetMeshScale);
            CapsuleComp->SetCapsuleHalfHeight(CapsuleComp->GetUnscaledCapsuleHalfHeight() / TargetMeshScale);
        }
        Character->SetActorScale3D(FVector(1.0f));
    }

    // 6. 정상 종료 시점에 공용 유틸기 15초 쿨타임을 확정 가동시킵니다
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}