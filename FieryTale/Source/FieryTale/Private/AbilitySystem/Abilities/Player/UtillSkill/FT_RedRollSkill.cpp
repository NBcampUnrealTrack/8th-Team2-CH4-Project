// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_RedRollSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h" // ◄ ASC 상호작용 배관망을 위해 인클루드 완착
#include "GameplayTags/FTTags.h"

UFT_RedRollSkill::UFT_RedRollSkill()
    : RollSpeed(1500.0f)     
    , RollDuration(0.35f)    
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 활성화 기간(구르는 동안) 동안 빨간 망토 무적/회피 상태 태그를 확정 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Evading);
}

void UFT_RedRollSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    // [쿨타임 완치 배관]: 구르기 기동에 진입하는 순간 CommitAbility 마스터 함수를 격발합니다.
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

    // 플레이어가 입력 중인 방향 패킷을 분석하여 순정 물리 이동 방향을 확보합니다.
    FVector RollDirection = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();

    // 만약 키보드 입력이 일절 없는 정지 상태라면 영웅의 정면 기준 180도 반대인 후방 구르기로 우회 처리합니다.
    if (RollDirection.IsNearlyZero())
    {
        RollDirection = -Character->GetActorForwardVector();
    }

    // 클라이언트와 서버간의 위치 동기화를 완벽하게 보장하는 등속도 직선 루트 모션을 주입합니다.
    UAbilityTask_ApplyRootMotionConstantForce* RootMotionTask = 
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this,
            FName("RedRollRootMotion"),
            RollDirection,
            RollSpeed,
            RollDuration,
            false,
            nullptr,
            ERootMotionFinishVelocityMode::SetVelocity,
            FVector::ZeroVector,
            0.0f,
            false
        );

    if (RootMotionTask)
    {
        // 구르기 제한 시간이 완결되면 안전 탈출 및 리소스 청소 콜백 배관을 연결합니다.
        RootMotionTask->OnFinish.AddDynamic(this, &UFT_RedRollSkill::OnRootMotionTimedOut);
        RootMotionTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

UGameplayEffect* UFT_RedRollSkill::GetCooldownGameplayEffect() const
{
    if (CooldownGameplayEffectClass)
    {
        return CooldownGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
    }

    return Super::GetCooldownGameplayEffect();
}

void UFT_RedRollSkill::OnRootMotionTimedOut()
{
    // 루트 모션 이동 시퀀스가 완결되면 어빌리티 공식 종료 관문으로 이동시킵니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_RedRollSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character && Character->GetCharacterMovement())
    {
        // 구르기 도중 하드 CC를 맞아 기술이 강제 캔슬당했다면 관성으로 인해 맵 밖으로 튕겨 나가는 현상을 막기 위해 캐릭터 물리 속도를 즉시 분쇄 동결합니다.
        if (bWasCancelled)
        {
            Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        }
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // =========================================================================
        // [AOS 기획 사양 락인 - CC기 파쇄 시 쿨타임 환불 처리]
        // 구르기 도중 적의 하드 CC기에 노출되어 강제 취소(bWasCancelled = true)되었다면,
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