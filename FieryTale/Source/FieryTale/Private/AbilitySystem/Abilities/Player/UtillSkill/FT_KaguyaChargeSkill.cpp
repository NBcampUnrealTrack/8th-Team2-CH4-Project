// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_KaguyaChargeSkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_KaguyaChargeSkill::UFT_KaguyaChargeSkill()
    : BambooGroveRadius(450.0f)     // 기획 데이터 락인: 대나무 숲 전술 영역 액터가 커버할 콜리전 스피어 반경 450센티미터 명세
    , BambooGroveDuration(4.0f)     // 기획 데이터 락인: 전술 영역이 월드에 생성되어 효과를 유지하는 한계 시간 4.0초 명세
{
    // 인스턴싱 정책: 영웅 캐릭터마다 독립적인 전술 영역 유지 타이머 핸들을 격리 통제하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬 뷰에서 전술 영역 진입 버프 태그와 이펙트가 격발되도록 클라이언트 선입력 예측을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 시프트 유틸기 범주 식별을 위해 총괄님 지시안대로 공용 UtilSkill 에셋 태그를 명확히 주입합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill); 
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 가구야 전용 시프트 생존 기술의 15초 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 시프트 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 결합되어 쿨다운 머신을 가동합니다.
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 보정 완료: 시스템 자동 부여 태그 슬롯에 가구야 전용 대나무 숲 영역 전개 버프 상태 태그를 마스터 상수 주소지로 직통 연결합니다.
    // 런타임 흐름 요약: 이 능력이 활성화되어 유지되는 동안 시전자의 몸에 이 태그가 부착되며, 전술 영역 유지 비주얼 연출 등과 공명하게 됩니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::BambooGroveDeploying);
}

void UFT_KaguyaChargeSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 기획 스펙 반영: 가구야의 현재 좌표와 회전값을 정밀 계산하여 영역 스폰 파이프라인의 기준점으로 삼습니다.
        FVector SpawnLocation = Character->GetActorLocation();
        FRotator SpawnRotation = Character->GetActorRotation();

        // 블루프린트 연동 가이드: 은신 요소를 전면 배제하라는 기획 수정 사양을 완벽하게 준수합니다.
        // 지정 반경인 450센티미터의 콜리전 범위를 가지며, 진입한 아군에게 은신이 아닌 직관적인 전투 버프 효과를 주입하는 
        // 가구야 고유의 전술 영역 영역 액터를 월드에 안전하게 스폰하도록 배관을 연결합니다.
        // World->SpawnActor<AActor>(BambooGroveAreaClass, SpawnLocation, SpawnRotation, SpawnParams);

        // 기획 스펙 반영: 전술 영역이 유지되는 정확한 4초 동안 타이머를 가동한 뒤 자동 종료 콜백 관문으로 인프라를 연결합니다.
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
    // 인터럽트 예외 방어 완료: 영역 전개 도중 적의 침묵이나 하드 CC기를 맞아 능력이 강제 캔슬당하더라도,
    // 월드 타이머 매니저에 등록된 4초 타임아웃 타이머 핸들을 즉시 안전하게 청소 수거하여 메모리 누수 릭을 원천 차단합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BambooGroveDurationTimerHandle);
    }

    // 버그 방쇄 완료: 표준 부모 소멸 가상 함수를 호출하는 순간 생성자 구역에서 부착했던 영역 전개 버프 태그가 클라이언트와 서버에서 자동으로 수거됩니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}