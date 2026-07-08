// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h" // ◄ ASC 상호작용 배관망을 위해 인클루드 완착
#include "GameplayTags/FTTags.h"

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

    // 활성화 기간 동안 앨리스 신체 축소 버프 태그를 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::ShrinkActive);
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    // [쿨타임 완치 배관]: 대시 시전에 진입하는 즉시 CommitAbility 마스터 함수를 격발합니다.
    // 이를 통해 시프트 Cooldown GE 장부가 오차 없이 시스템에 안착합니다.
    // =========================================================================
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

    // =========================================================================
    // 💡 [이속 장부 파괴 릭 완치 - GAS 순정 대시 가속 라인 연동]
    // MaxWalkSpeed 변수를 직접 조지는 원시 하드코딩을 소각하고, 다른 버프/디버프와 
    // 정밀 가산/곱산 중첩 연산이 가능하도록 대시 가속(1.5배 증가) GE를 주입합니다.
    // =========================================================================
    if (DashSpeedGameplayEffectClass)
    {
        FGameplayEffectSpecHandle SpeedSpecHandle = MakeOutgoingGameplayEffectSpec(DashSpeedGameplayEffectClass, GetAbilityLevel());
        if (SpeedSpecHandle.IsValid())
        {
            DashSpeedActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpeedSpecHandle.Data.Get());
        }
    }
    
    // 시각적 연동: 캐릭터 비주얼 메시 스케일을 기획 스펙에 맞추어 축소합니다.
    Character->SetActorScale3D(FVector(TargetMeshScale));

    // 2초 버프 지속 제한 시간 타이머 가동
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
    // 2초 지속 시간 만료 시 정상 종료 마감 처리
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 청정 소각하여 레이스 컨디션 차단
    if (GetWorld() && ShrinkBuffDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
        ShrinkBuffDurationTimerHandle.Invalidate();
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character)
    {
        // 캐릭터 메시 스케일을 다시 순정 1대1대1 규격으로 원상 복귀시킵니다.
        Character->SetActorScale3D(FVector(1.0f));
    }

    if (SourceASC)
    {
        // 대시 가속 GE를 제거하여 속도를 원래 스탯 상태로 무결하게 환원합니다.
        if (DashSpeedActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(DashSpeedActiveHandle);
            DashSpeedActiveHandle.Invalidate();
        }

        // =========================================================================
        // [AOS 기획 사양 락인 - CC기 파쇄 시 쿨타임 환불 처리]
        // 대시 지속 도중 적의 하드 CC기에 노출되어 강제 취소(bWasCancelled = true)되었다면,
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