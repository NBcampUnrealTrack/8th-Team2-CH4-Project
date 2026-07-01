// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_KaguyaChargeSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaChargeSkill::UFT_KaguyaChargeSkill()
    : BambooGroveRadius(450.0f)     // 기획 스펙: 연막 안개 영역 반경 450cm
    , BambooGroveDuration(4.0f)     // 기획 스펙: 안개 영역 유지 시간 4.0초
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Shift 유틸기 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // Shift 이동/생존 기술 공용 재사용 대기시간 태그 매핑 (데이터 에셋 설정에 따라 쿨타임 가동)
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 연막 안개를 전개하는 동안 일시적인 버프 또는 시전 상태를 나타내는 태그 설정 구역
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.BambooGroveDeploying")));
}

void UFT_KaguyaChargeSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 스킬을 쓸 수 있는 자원 상태를 검사하고 승인 처리합니다
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 기획 스펙 반영: 가구야의 현재 정면 지정 위치(또는 전방 궤적 범위) 계산
        FVector SpawnLocation = Character->GetActorLocation();
        FRotator SpawnRotation = Character->GetActorRotation();

        // 블루프린트 또는 전용 클래스 연동 포인트
        // 지정 반경(450cm)의 콜리전 스피어를 가지고 진입한 아군에게 은신(Invisibility) GE를 부여하는 
        // 전술 영역 연막 액터(예: AFT_BambooGroveArea)를 월드에 생성합니다.
        // World->SpawnActor<AActor>(BambooGroveAreaClass, SpawnLocation, SpawnRotation, SpawnParams);

        // 기획 스펙 반영: 안개 영역이 전개되는 4초 동안 타이머를 유지한 뒤 자동 종료 파이프라인으로 연결합니다
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

    // 추후 구현 포인트 황금 대나무 잎이 휘날리는 짙은 연막 안개 나이아가라 파티클 시스템 가동 구역
}

void UFT_KaguyaChargeSkill::OnChargeFinished()
{
    // 4초 지속이 완료되는 즉시 어빌리티를 마감하고 정리 관문으로 진입합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaChargeSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 지속 타이머를 안전하게 제거합니다
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BambooGroveDurationTimerHandle);
    }

    // 정상적으로 4초 유지를 마치고 종료되었을 때 시스템 공용 유틸기 쿨타임을 확정 발동시킵니다
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}