// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)
    , FlySpeedPenaltyMultiplier(0.7f)
    , OriginalMaxWalkSpeed(0.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Flying);
}

void UFT_AladdinFlySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
    
    if (!Character || !MoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    // 비행 페널티 연산 적용 직전 캐릭터의 순정 최대 걷기 속도를 보관소에 안전 백업합니다.
    OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
    
    // 기획 스펙 반영: 비행 전개에 따른 고유 비행 속도 수치를 배정하고 최대 속도 통제선에 다이렉트 주입합니다.
    MoveComp->MaxFlySpeed = OriginalMaxWalkSpeed * FlySpeedPenaltyMultiplier;
    MoveComp->MaxWalkSpeed = MoveComp->MaxFlySpeed;
    
    // 무브먼트 스위칭: 지면 마찰과 중력의 물리 영향을 전면 끄고 공중 비행 자유 모드로 강제 제어합니다.
    MoveComp->SetMovementMode(EMovementMode::MOVE_Flying);

    // 공중으로 떠오르는 물리 부력 연출을 순간 격발합니다.
    Character->LaunchCharacter(FVector(0.0f, 0.0f, 250.0f), false, true);
    
    // 기획 스펙 반영: 무한 비행 치트를 방지하기 위해 5초 지속 제한 시간 타임아웃 타이머를 가동합니다.
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
    // 5초 지속 시간이 안전하게 모두 만료되면 표준 종료 마감 관문으로 상태를 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinFlySkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 누수 안전 해제: 능력이 닫히기 시작하면 가동 중이던 5초 지속시간 타이머 핸들을 즉시 청소 수거하고 무효화합니다.
    if (GetWorld() && FlyDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(FlyDurationTimerHandle);
        FlyDurationTimerHandle.Invalidate();
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    if (Character && Character->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
        
        // 인터럽트 예외 방어: 비행 지속시간 도중 적의 침묵이나 하드 CC기를 맞아 능력이 강제 캔슬당하더라도,
        // 무조건 중력의 영향을 받는 순정 걷기 모드로 안전 착륙 복귀 조치합니다.
        MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
        
        // 이속 복구 확정: 처음에 안전 백업해 두었던 순정 원본 속도 수치 그대로 최대 걷기 속도 배관에 1대1 환원 복구시킵니다.
        if (OriginalMaxWalkSpeed > 0.0f)
        {
            MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
        }
    }

    // AOS 쿨다운 정책 완착: 양탄자 비행이 정상 종료되어 안전하게 지상 착륙을 완료한 바로 이 최종 시점에만 15초 고유 쿨타임을 발동시킵니다.
    // 만약 적의 하드 CC기에 의해 비행 도중 강제 취소당한 억까 상황이라면 쿨타임 패널티 없이 리턴 복귀합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}