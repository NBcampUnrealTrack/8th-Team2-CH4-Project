// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AliceStealthDashSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AliceStealthDashSkill::UFT_AliceStealthDashSkill()
    : MovementSpeedMultiplier(1.5f) // 기획 데이터 락인: 스킬 전개 즉시 기본 이동 속도를 50퍼센트 증가시키는 배율 명세
    , MaxDuration(2.0f)             // 기획 데이터 락인: 축소 가속 상태를 정확히 2초간 유지하는 한계 시간 명세
    , TargetMeshScale(0.5f)         // 기획 데이터 락인: 피격 면적을 줄이기 위해 히트박스와 캐릭터 스케일을 절반 크기로 축소하는 수치 명세
    , OriginalMaxWalkSpeed(0.0f)    // 이동 속도 복구 연산 왜곡을 차단하기 위한 기본 수치 보관소 초기화
    , OriginalCapsuleRadius(0.0f)
    , OriginalCapsuleHalfHeight(0.0f)
{
    // 인스턴싱 정책: 캐릭터마다 고유의 캡슐 크기 백업 정보와 타이머 핸들을 격리 제어하기 위해 인스턴스화합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬 뷰에서 몸집이 작아지며 빠르게 치고 나가도록 클라이언트 선입력 예측을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 시프트 이동 생존 기술 범주 식별을 위해 공용 에셋 태그를 마킹합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtilSkill);
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 앨리스 전용 시프트 생존 기술의 15초 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 시프트 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 1대1 결합됩니다.
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 버프 유지 시간 동안 캐릭터에게 적용할 전용 상태 태그 바인딩 구역입니다.
    // 은밀 또는 은신 요소를 전면 배제하라는 기획 수정 사양에 맞추어 오직 신체 축소 상태 버프 태그만 락인합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::ShrinkActive);
}

void UFT_AliceStealthDashSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 이속 버그 방어선 수술: 런타임 나누기 연산 오차로 기본 속도가 증발하는 릭을 원천 차단하기 위해
    // 가속 전 캐릭터의 순정 최대 걷기 속도 원본을 보관소에 안전하게 선제 백업합니다.
    // 백업이 완료되면 기획 스펙인 50퍼센트 가속 수치를 연동 주입합니다.
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
        MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * MovementSpeedMultiplier;
    }

    // 기획 스펙 반영: 앨리스의 히트박스 캡슐 콜리전 축소 및 원본 수치 임시 백업 보정 구역입니다.
    if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
    {
        // 실시간 역연산 물리 마찰 오차를 방지하기 위해 축소 전 순정 원본 크기를 완벽하게 백업합니다.
        OriginalCapsuleRadius = CapsuleComp->GetUnscaledCapsuleRadius();
        OriginalCapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();

        // 안전하게 정밀 타겟 수치인 절반 크기로 캡슐 실질 충돌 범위를 축소 단행합니다.
        CapsuleComp->SetCapsuleSize(OriginalCapsuleRadius * TargetMeshScale, OriginalCapsuleHalfHeight * TargetMeshScale, true);
    }
    
    // 시각적 연동: 캐릭터 메시 비주얼도 기획 스펙에 맞추어 절반 크기로 축소 스케일링합니다.
    Character->SetActorScale3D(FVector(TargetMeshScale));

    // 기획 스펙 반영: 무한 축소 치트를 방지하기 위해 정확한 2초 버프 지속 제한 시간 타이머를 가동합니다.
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
    // 2초 지속 시간이 모두 정상 만료되면 표준 종료 마감 관문으로 상태를 이관합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceStealthDashSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 누수 안전 해제: 능력이 닫히기 시작하면 가동 중이던 2초 지속시간 타이머 핸들을 즉시 청소 수거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShrinkBuffDurationTimerHandle);
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character)
    {
        // 이속 복구 확정 수술: 복사 나눗셈 연산 왜곡 없이 처음에 안전 백업해 두었던 순정 원본 속도 수치 그대로 최대 걷기 속도 배관에 1대1 환원 복구시킵니다.
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (OriginalMaxWalkSpeed > 0.0f)
            {
                MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
            }
        }

        // 물리 크래시 박멸 완료: 백업해 두었던 원본 정수로 캡슐 규격을 고정 초기화하여 지형에 끼이거나 캐릭터가 찌그러지는 현상을 소멸시킵니다.
        if (UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent())
        {
            if (OriginalCapsuleRadius > 0.0f && OriginalCapsuleHalfHeight > 0.0f)
            {
                CapsuleComp->SetCapsuleSize(OriginalCapsuleRadius, OriginalCapsuleHalfHeight, true);
            }
        }
        
        // 캐릭터 메시 스케일을 다시 순정 1대1대1 규격으로 안전하게 회귀시킵니다.
        Character->SetActorScale3D(FVector(1.0f));
    }

    // 버그 완벽 차단: 표준 부모 소멸 가상 함수를 호출하는 순간 생성자 구역에서 부착했던 신체 축소 버프 태그가 자동으로 깔끔하게 소멸합니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}