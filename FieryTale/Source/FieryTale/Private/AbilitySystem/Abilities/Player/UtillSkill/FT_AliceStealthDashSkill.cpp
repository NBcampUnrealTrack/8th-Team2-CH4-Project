// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : MaxDuration(2.0f)
    , TargetMeshScale(0.5f)
{
    // 인스턴싱 정책 및 network 실행 정책을 로컬 예측 규격으로 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 고유 자산 태그 등록 및 쿨타임 추적용 태그 매핑을 수행합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 활성화 기간 동안 앨리스 신체 축소 버프 태그를 소유 태그로 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::ShrinkActive);
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 자원 커밋 일원화: 불필요한 중복 검문소를 폐쇄하고 CommitAbility 단일 파이프라인으로 일원화하여 대시 시전 시점에 자원을 원자적으로 확정합니다.
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

    // 이속 장부 파괴 릭 완치: MaxWalkSpeed 변수를 직접 변경하는 하드코딩을 배제하고 GAS 순정 가산/곱산 중첩 연산이 가능하도록 대시 가속 이펙트를 주입합니다.
    if (DashSpeedGameplayEffectClass)
    {
        FGameplayEffectSpecHandle SpeedSpecHandle = MakeOutgoingGameplayEffectSpec(DashSpeedGameplayEffectClass, GetAbilityLevel());
        if (SpeedSpecHandle.IsValid())
        {
            DashSpeedActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpeedSpecHandle.Data.Get());
        }
    }

    // 신체 축소: 캡슐과 메시를 기본값 기준으로 일관되게 축소하기 위해 CharacterBase가 소유한 단일 진입점을 구동합니다.
    Character->SetCharacterScale(TargetMeshScale);

    // 카메라 거리 보정: 축소 연출과 별개로 카메라 거리를 같은 배율로 당겨, 축소 전과 동일한 상대 거리(체감 크기)를 유지시킵니다.
    Character->SetCameraDistanceScale(TargetMeshScale);

    // 버프 지속 제한 시간 타이머 가동을 통해 수명을 관리합니다.
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
    // 지속 시간 정상 만료에 의한 클린 마감 분기입니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 메모리 누수 방지: 런타임 수명선 보장용 지속 시간 타이머 장부를 청정 소각합니다.
    if (GetWorld() && ShrinkBuffDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
        ShrinkBuffDurationTimerHandle.Invalidate();
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (Character)
    {
        // 신체 스케일을 CharacterBase가 캐싱해 둔 오리지널 기본값 기준으로 무결하게 원복합니다.
        Character->ResetCharacterScale();

        // 카메라 거리도 BeginPlay 시점의 최초 길이로 원복 (스케일 원복과 독립적으로 동작)
        Character->ResetCameraDistanceScale();
    }

    if (SourceASC)
    {
        // 대시 가속 GE를 완벽히 해제하여 캐릭터 이동 속도를 원래 스탯 상태로 안전하게 환원합니다.
        if (DashSpeedActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(DashSpeedActiveHandle);
            DashSpeedActiveHandle.Invalidate();
        }

        // 네트워크 동기화 쿨타임 환불: 대시 지속 도중 하드 CC기나 상태이상으로 강제 캔슬된 경우 예측적으로 돌아가던 쿨타임 이펙트를 즉각 회수합니다.
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);

            // 엔진 공식 순정 팩토리 함수인 MakeQuery_MatchAnyEffectTags 제어선으로 단일화하여 무기 스왑이나 강제 인터럽트 시 발생하는 먹통 결함을 원천 진압합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}