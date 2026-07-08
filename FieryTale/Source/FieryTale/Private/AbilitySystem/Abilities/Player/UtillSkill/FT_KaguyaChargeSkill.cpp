// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_KaguyaChargeSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h" // ◄ ASC 상호작용 배관망을 위해 인클루드 완착
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

    // 활성화 기간 동안 영역 전개 중 버프 태그를 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::BambooGroveDeploying);
}

void UFT_KaguyaChargeSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 1단계: 시프트 생존기 쿨타임 검문
    if (!CheckCooldown(Handle, ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // =========================================================================
    // [쿨타임 완치 배관]: 안개 전개에 진입하는 순간 CommitAbility 마스터 함수를 격발합니다.
    // 이를 통해 시프트 Cooldown GE 장부가 오차 없이 시스템에 안착합니다.
    // =========================================================================
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

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

        // =========================================================================
        // [영역 액터 가동 파이프라인 개통]: 에디터 슬롯(BambooGroveAreaClass) 검문선 타설
        // 에디터에서 전술 안개 영역 블루프린트 액터가 정상 바인딩되어 있다면 월드에 소환합니다.
        // =========================================================================
        if (BambooGroveAreaClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Character;
            SpawnParams.Instigator = Character;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // 💡 추후 이 소환된 영역 액터 내부에 BambooGroveRadius와 BambooGroveDuration을 전달하여 
            // 시각적 파티클 크기와 콜리전 볼륨 범위가 동기화되도록 연동하시면 청정합니다.
            World->SpawnActor<AActor>(BambooGroveAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
        }

        // 4초 전술 안개 영역 유지 타이머 가동
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
    // 4초 지속 시간 만료 시 정상 종료 마감 처리
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaChargeSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 청정 소각하여 레이스 컨디션 차단
    if (GetWorld() && BambooGroveDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(BambooGroveDurationTimerHandle);
        BambooGroveDurationTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // =========================================================================
        // [AOS 기획 사양 락인 - CC기 파쇄 시 쿨타임 환불 처리]
        // 안개 전개 도중 적의 하드 CC기에 노출되어 강제 취소(bWasCancelled = true)되었다면,
        // 선제 적용되었던 시프트 쿨타임 이펙트를 찾아내 삭제하여 리턴 복귀를 보장합니다.
        // =========================================================================
        if (bWasCancelled)
        {
            FGameplayEffectQuery UniversalQuery;
            TArray<FActiveGameplayEffectHandle> ActiveHandles = SourceASC->GetActiveEffects(UniversalQuery);

            for (const FActiveGameplayEffectHandle& GEPipeHandle : ActiveHandles)
            {
                const FActiveGameplayEffect* ActiveGE = SourceASC->GetActiveGameplayEffect(GEPipeHandle);
                if (ActiveGE && ActiveGE->Spec.Def)
                {
                    if (ActiveGE->Spec.Def->GetAssetTags().HasTagExact(CooldownTag) || 
                        ActiveGE->Spec.Def->GetGrantedTags().HasTagExact(CooldownTag))
                    {
                        SourceASC->RemoveActiveGameplayEffect(GEPipeHandle);
                    }
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}