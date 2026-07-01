// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)                 // 기획 데이터 락인: 양탄자를 타고 공중에 비행을 유지할 수 있는 한계 시간 5초 명세
    , FlySpeedPenaltyMultiplier(0.7f)   // 기획 데이터 락인: 비행 중 사격 및 공중 선회 밸런스를 위한 이동 속도 30퍼센트 감산 배율 명세
    , OriginalMaxWalkSpeed(0.0f)        // 런타임 이속 복구 연산 오차를 방지하기 위한 백업 보관소 초기화
{
    // 인스턴싱 정책: 캐릭터마다 개별적인 비행 유지 타이머 핸들과 백업 이속 수치를 격리 통제하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬 뷰에서 공중 부력 연출과 비행 상태로 진입하도록 클라이언트 선입력 예측 시스템을 기동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 시프트 유틸기 범주 식별을 위해 전용 에셋 태그를 마킹합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 알라딘 전용 시프트 생존 기술의 15초 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 시프트 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 결합되어 쿨다운 머신을 가동합니다.
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 보정 완료: 시스템 자동 부여 태그 슬롯에 알라딘 전용 공중 비행 상태 버프 태그를 상시 락인합니다.
    // 런타임 흐름 요약: 이 능력이 활성화되어 유지되는 동안 시전자의 몸에 이 태그가 부착됩니다.
    // 이 태그를 기반으로 애니메이션 블루프린트에서 양탄자 탑승 모션을 출력하거나, 특정 대공 추가 피해 판정 등과 공명하게 됩니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::Flying);
}

void UFT_AladdinFlySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // GAS 마스터 규격: 시프트 스킬 쿨다운 상태를 선제 검사하고 자원 비용을 서버와 클라이언트에서 동시 승인 소모합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        // 쿨타임 중이거나 기절 상태라면 즉시 어빌리티 수명주기를 안전 종료하여 좀비 잔존을 차단합니다.
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

    // 이속 버그 방어선 1단계: 비행 페널티 연산 적용 직전 캐릭터의 순정 최대 걷기 속도를 보관소에 안전 백업합니다.
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
    // 타이머 누수 안전 해제: 능력이 닫히기 시작하면 가동 중이던 5초 지속시간 타이머 핸들을 즉시 청소 수거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FlyDurationTimerHandle);
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    if (Character && Character->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
        
        // 인터럽트 예외 방어: 비행 지속시간 도중 적의 침묵이나 하드 CC기를 맞아 능력이 강제 캔슬당하더라도,
        // 무조건 중력의 영향을 받는 순정 걷기 모드(MOVE_Walking)로 안전 착륙 복귀 조치합니다.
        MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
        
        // 이속 복구 확정: 나눗셈 연산 왜곡 없이 처음에 안전 백업해 두었던 순정 원본 속도 수치 그대로 최대 걷기 속도 배관에 1대1 환원 복구시킵니다.
        if (OriginalMaxWalkSpeed > 0.0f)
        {
            MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
        }
    }

    // 버그 차단 완료: 표준 부모 소멸 가상 함수를 호출하는 순간 생성자 구역에서 부착했던 Flying 버프 태그가 클라이언트와 서버에서 동시에 자동 소멸합니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}