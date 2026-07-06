// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_KaguyaBulwarkAbility.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaBulwarkAbility::UFT_KaguyaBulwarkAbility()
    : MaxDuration(4.0f) // 기획 명세: 대나무 방벽 최대 유지 제한 시간 (4초)
{
    // 인스턴싱 정책: 방벽 실시간 수명 주기 관리 및 데이터 격리를 위해 액터당 인스턴스 생성
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선예측 구동 후 서버 검증으로 가드 입력 반응성 극대화
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // GAS 에셋 태그 등록: 스킬 분류 장부에 보조 공격 기술 태그 바인딩
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 쿨다운 태그 지정: 마우스 우클릭 스킬 공용 쿨다운 파이프라인과 연동
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;

    // 방벽 전개 동안 가구야 본체에 반격 가드 태세 버프 태그를 상시 락인 (피해 연산기 공명)
    // ActivationOwnedTags에 등록된 태그는 스킬 시전 동안 자동으로 부여되고 종료 시 자동 수거됩니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::CounterReady);
}

void UFT_KaguyaBulwarkAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [쿨다운 및 비용 선제 검증 관문]
    // 실패 시 아래의 이속 조작 단계를 거치지 않고 순정 상태로 클린 탈출하여 이속 마비 버그를 원천 차단합니다.
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

    // 채널링 상태 표식 주입 : 방벽 유지 동안 다른 액티브 스킬이나 일반 평타 조작을 차단하는 파이프라인 연동
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    
    // [이동 속도 감산 페널티 적용]
    // 기획서 명세에 따라 방벽을 든 상태에서 느리게 걷도록 설계된 이속 저하 GameplayEffect를 주입합니다.
    if (MovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            // 종료 시점에 확정 수거하기 위해 활성화 핸들을 멤버 변수에 저장합니다.
            MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }

    // [비동기 몽타주 재생 및 인터럽트 태스크 가동]
    // 무기 데이터 에셋에 등록된 대나무 가드 전용 몽타주 애니메이션을 루프 구동합니다.
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    if (WeaponData && WeaponData->AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), WeaponData->AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 가드 모션이 정상 완료되었거나 피격 등으로 인터럽트되었을 때의 마감 콜백 바인딩
            MontageTask->OnCompleted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardMontageFinished);
            MontageTask->ReadyForActivation();
        }
    }

    // [최대 유지 제한 지속 타이머 가동]
    // 마우스를 계속 누르고 있어도 최대 4초가 지나면 무한 가드 악용을 차단하기 위해 강제로 능력을 닫는 안전장치입니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            BulwarkDurationTimerHandle, 
            FTimerDelegate::CreateUObject(this, &UFT_KaguyaBulwarkAbility::EndAbility, Handle, ActorInfo, ActivationInfo, true, false),
            MaxDuration, 
            false
        );
    }
}

void UFT_KaguyaBulwarkAbility::OnGuardMontageFinished()
{
    // 애니메이션 제어가 끝난 시점에 즉시 안전 종료 단계로 수명 주기 토스
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaBulwarkAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // [안전장치 1] 타이머 핸들이 유효할 때만 안전하게 클리어합니다.
    if (GetWorld() && BulwarkDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
        BulwarkDurationTimerHandle.Invalidate(); // 핸들 메모리까지 깔끔하게 초기화
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
        
        // [안전장치 2] 이미 제거된 GE 효과를 중복 제거하려다 발생하는 경고 로그를 차단합니다.
        if (MovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
            MovementPenaltyActiveHandle.Invalidate();
        }
    }

    // [쿨다운 확정 마감]
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}