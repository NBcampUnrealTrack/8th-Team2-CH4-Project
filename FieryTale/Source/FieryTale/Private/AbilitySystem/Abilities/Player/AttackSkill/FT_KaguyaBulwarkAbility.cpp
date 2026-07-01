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
    : MovementPenaltyMultiplier(0.6f) // 기획 데이터 락인: 방벽 전개 중 이동 속도 40퍼센트 감소 페널티 (기본 속도의 60퍼센트 유지)
    , MaxDuration(4.0f)              // 기획 데이터 락인: 마우스 우클릭 강공격을 최대로 전개 유지할 수 있는 한계 시간 4초 명세
    , OriginalMaxWalkSpeed(0.0f)
{
    // 인스턴싱 정책: 캐릭터마다 방벽 전개 수명 주기 및 백업 이속 수치를 격리 통제하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 지연 없이 방패를 내리찍도록 클라이언트 선입력 예측 시스템을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 우클릭 보조 공격 범주 식별을 위해 공용 AttackSkill 에셋 태그를 명확히 마킹합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 에셋 바인딩 가이드: 가구야 전용 보조 공격의 6초 재사용 대기시간 태그 매핑입니다.
    // 기획 공식 반영: 방벽을 켜자마자 쿨이 도는 릭을 차단하고, 방벽을 정상 해제하여 내리는 시점부터 수동 쿨다운 연산기를 가동합니다.
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;

    // 기획 스펙 반영: 방벽 전개 동안 가구야 본체에 반격 가드 태세 버프 태그를 상시 락인합니다.
    // 런타임 흐름 요약: 이 태그가 유지되는 동안 글로벌 피해 연산기(GEEC_Damage) 내부의 전방 120도 판정 필터와 자석처럼 공명하여 피해를 전면 소멸시킵니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::CounterReady);
}

void UFT_KaguyaBulwarkAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // GAS 마스터 규격: 부모 가상 함수 호출 및 실질 액션 전개 이전에 우클릭 6초 쿨다운 상태를 최선행 검증 처리합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!Character || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 채널링 상태 표식 주입: 방벽 전개 중임을 실시간 전파하여 기획 사양인 공격 및 다른 스킬 사용 시 방벽이 즉시 해제되도록 차단 파이프라인의 기준 태그를 세웁니다.
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

    // 이속 버그 방어선 수술: 런타임 나누기 연산 오차로 기본 속도가 유실되는 릭을 원천 차단하기 위해
    // 가속 페널티 적용 전 캐릭터의 순정 최대 걷기 속도 원본을 보관소에 안전하게 선제 백업한 뒤 40퍼센트 감산 수치를 연동 주입합니다.
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
        MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * MovementPenaltyMultiplier;
    }

    // 몽타주 비동기 태스크 구동: 가구야 무기 데이터 에셋의 AttackMontage 슬롯에 등록된 황금 대나무 장벽 전개 애니메이션 루프를 격발합니다.
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    if (WeaponData && WeaponData->AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), WeaponData->AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 애니메이션 몽타주가 정상 완결되거나 다른 인풋 인터럽트에 의해 중단 신호를 수신하면 안전 해제 파이프라인으로 연결합니다.
            MontageTask->OnCompleted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardMontageFinished);
            MontageTask->ReadyForActivation();
        }
    }

    // 최대 4초 유지 제한 지속 타이머 가동: 마우스 우클릭을 떼지 않고 무한정 버티는 악용 치트를 막기 위해 4초 도달 시 자동으로 닫히도록 안전 벨브를 엽니다.
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
    // 애니메이션이 끝나거나 마우스 릴리즈 중단 신호를 수신하면 즉시 표준 어빌리티 종료 마감 관문으로 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaBulwarkAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 누수 안전 해제: 능력이 종료되기 시작하면 가동 중이던 4초 한계 지속시간 타이머 핸들을 즉시 청소 수거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
    }

    // 시전자 몸에 임시 부착해 두었던 채널링 상태 표식 태그를 깔끔하게 제거 수거합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 이속 복구 확정 수술: 복사 나누기 연산 왜곡 없이 처음에 안전 백업해 두었던 순정 원본 속도 수치 그대로 최대 걷기 속도 배관에 1대1 환원 복구시킵니다.
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

    // 기획 스펙 공식 반영: 적의 강제 CC기로 캔슬당한 하차 상태가 아니라, 플레이어가 방벽을 정상적으로 내리는 해제 시점부터 우클릭 6초 쿨타임을 가동합니다.
    if (!bWasCancelled)
    {
        CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }
    
    // 최종 부모 소멸 시퀀스를 격발하여 가구야의 몸집에 부착되어 있던 CounterReady 반격 가드 버프 태그를 서버와 클라이언트에서 자동 수거합니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}