// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_CounterShieldSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_CounterShieldSkill::UFT_CounterShieldSkill()
    : MovementPenaltyMultiplier(0.6f) // 기획 데이터 락인: 방벽 전개 중 이동 속도 40퍼센트 감소 페널티 (기본 속도의 60퍼센트 유지)
    , MaxDuration(4.0f)              // 기획 데이터 락인: 마우스 우클릭을 계속 유지할 수 있는 최대 지속 한계 시간 4초 명세
    , OriginalMaxWalkSpeed(0.0f)
{
    // 인스턴싱 정책: 캐릭터마다 방벽 유지 시간 및 백업 이속 수치를 개별 제어하기 위해 인스턴스화 모드로 작동합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬에서 방벽 모션을 띄우도록 클라이언트 선입력 예측 시스템을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 에셋 바인딩 가이드: 가구야 전용 우클릭 보조 공격의 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 우클릭 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 결합됩니다.
    // 가구야는 방벽을 켜자마자 쿨이 도는 것이 아니라 방벽을 해제하여 내리는 시점에 수동으로 격발시킵니다.
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;

    // 보정 완료: 시스템 자동 부여 태그 슬롯에 가구야 전용 반격 가드 태세 버프 태그를 상시 락인합니다.
    // 런타임 흐름 요약: 이 스킬이 활성화되어 유지되는 동안 시전자의 몸에 이 태그가 부착되어 있습니다.
    // 피해 연산기(GEEC_Damage)는 공격을 연산하기 직전 피격자 몸에 이 태그가 발견되면 피해량을 0으로 지우고 튕겨냅니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::CounterReady);
}

void UFT_CounterShieldSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
    // 쿨타임 검증: 쿨다운 중이거나 침묵 등 군중제어에 걸려 있다면 방벽 시전 관문에서 즉시 차단합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    
    if (!Character || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 채널링 상태 태그를 루즈 태그로 주입하여 다른 행동 컴포넌트에 현재 방벽 유지 중임을 실시간 전파합니다.
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

    // 이속 버그 방어선 1단계: 런타임 역연산 왜곡을 막기 위해 방벽 전개 전 캐릭터의 순정 최대 속도를 보관소에 안전 백업합니다.
    // 백업 완료 후 기획 스펙에 맞추어 현재 최고 속도를 40퍼센트 깎아 묵직한 방벽 이동 상태를 물리 구현합니다.
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
        MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * MovementPenaltyMultiplier;
    }

    // 몽타주 비동기 태스크 구동 구역
    // 에디터 세팅: 가구야 무기 데이터 에셋(DA_WeaponData_Kaguya)의 AttackMontage 슬롯에 방벽 유지 루프 애니메이션을 등록해야 합니다.
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    if (WeaponData && WeaponData->AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), WeaponData->AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 애니메이션이 무사히 끝나거나 외부 인풋 릴리즈 신호로 중단 시 해제 파이프라인으로 연결합니다.
            MontageTask->OnCompleted.AddDynamic(this, &UFT_CounterShieldSkill::OnGuardMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_CounterShieldSkill::OnGuardMontageFinished);
            MontageTask->ReadyForActivation();
        }
    }

    // 최대 4초 유지 제한 지속 타이머 가동: 마우스를 떼지 않고 영구 버티는 치트를 막기 위해 4초 도달 시 자동으로 닫히도록 안전 밸브를 엽니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            BulwarkDurationTimerHandle, 
            FTimerDelegate::CreateUObject(this, &UFT_CounterShieldSkill::EndAbility, Handle, ActorInfo, ActivationInfo, true, false),
            MaxDuration, 
            false
        );
    }
}

void UFT_CounterShieldSkill::OnGuardMontageFinished()
{
    // 애니메이션 몽타주가 끝나거나 중단 신호를 수신하면 표준 어빌리티 종료 관문으로 안전하게 상태를 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_CounterShieldSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 누수 완전 소멸: 능력이 닫히기 시작하면 가동 중이던 4초 한계선 타이머 핸들을 즉시 청소합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
    }

    // 시전자 몸에 부착해 두었던 채널링 상태 표식 태그를 안전하게 수거합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 이속 원상복구 확정: 복사 나눗셈 계산 오차 없이 처음에 안전 백업해 두었던 순정 속도 수치 그대로 물리 컴포넌트에 즉시 환원시킵니다.
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                if (OriginalMaxWalkSpeed > 0.0f)
                {
                    MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
                }
            }
        }
    }

    // 기획 스펙 공식 반영: 적의 강제 CC기로 캔슬당한 하차 상태가 아니라, 가구를 정상적으로 내리는 해제 시점부터 우클릭 6초 쿨타임을 가동합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }
    
    // 최종 정리 수명주기 관문 작동
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}