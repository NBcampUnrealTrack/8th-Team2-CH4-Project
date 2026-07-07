// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_KaguyaChargeSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaChargeSkill::UFT_KaguyaChargeSkill()
    : BambooGroveRadius(450.0f)
    , BambooGroveDuration(4.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill); 
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::BambooGroveDeploying);
}

void UFT_KaguyaChargeSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        FVector SpawnLocation = Character->GetActorLocation();
        FRotator SpawnRotation = Character->GetActorRotation();

        // 블루프린트 연동 가이드: 에디터 단에서 BambooGroveAreaClass 슬롯에 실제 안개 이펙트와 범위 콜리전이 탑재된 BP 액터를 바인딩하십시오.
        // World->SpawnActor<AActor>(BambooGroveAreaClass, SpawnLocation, SpawnRotation, SpawnParams);

        // 기획 스펙 반영: 전술 안개 영역이 유지되는 정확한 4초 동안 타이머를 가동한 뒤 자동 종료 콜백 관문으로 인프라를 연결합니다.
        World->GetTimerManager().SetTimer(
            BambooGroveDurationTimerHandle,
            this,
            &UFT_KaguyaChargeSkill::OnChargeFinished,
            BambooGroveDuration,
            false
        );
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_KaguyaChargeSkill::OnChargeFinished()
{
    // 4초 지속 시간이 제한 없이 완벽하게 만료되면 표준 종료 마감 관문으로 상태를 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaChargeSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 예외 방어 완료: 영역 전개 도중 적의 침묵이나 하드 CC기를 맞아 능력이 강제 캔슬당하더라도 월드 타이머 매니저에 등록된 타이머 핸들을 즉시 안전하게 청소 수거하고 무효화합니다.
    if (GetWorld() && BambooGroveDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(BambooGroveDurationTimerHandle);
        BambooGroveDurationTimerHandle.Invalidate();
    }

    // AOS 쿨다운 정책 완착: 전술 안개 영역 전개가 완전히 종료되어 마감된 바로 이 최종 시점에만 15초 고유 쿨타임을 발동시킵니다.
    // 만약 적의 하드 CC기에 의해 전개 도중 강제 취소당한 억까 상황이라면 쿨타임 패널티 없이 리턴 복귀합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}