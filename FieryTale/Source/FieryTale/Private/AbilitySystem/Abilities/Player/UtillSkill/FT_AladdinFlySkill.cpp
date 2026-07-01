// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(5.0f)                 // 기획 스펙: 5초 비행 유지
    , FlySpeedPenaltyMultiplier(0.7f)   // 기획 스펙: 이동 속도 30% 감산 (0.7배)
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Shift 유틸기 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // Shift 이동/생존 기술 공용 재사용 대기시간 태그 매핑 (데이터 에셋 설정상 알라딘은 15초 쿨타임 작동)
    CooldownTag = FTTags::FTStates::Cooldown::UtilSkill;

    // 비행 상태 동안 부여할 전용 태그
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.Flying")));
}

void UFT_AladdinFlySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
    
    if (!Character || !MoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 기획 스펙 반영: 비행 전개 시 원래 이동 속도의 30% 감산 적용
    MoveComp->MaxFlySpeed = MoveComp->MaxWalkSpeed * FlySpeedPenaltyMultiplier;
    MoveComp->MaxWalkSpeed = MoveComp->MaxFlySpeed;
    
    // 지면 마찰과 중력의 영향을 끄고 비행 모드로 강제 설정
    MoveComp->SetMovementMode(EMovementMode::MOVE_Flying);

    // 공중으로 떠오르는 부력 연출
    Character->LaunchCharacter(FVector(0.0f, 0.0f, 250.0f), false, true);

    // 기획 스펙 반영: 5초간의 비행 유지 타이머 가동
    GetWorld()->GetTimerManager().SetTimer(
        FlyDurationTimerHandle, 
        this, 
        &UFT_AladdinFlySkill::OnFlyDurationExpired, 
        FlyDuration, 
        false
    );

    // 추후 구현 포인트 양탄자 메쉬 부착 및 기류 바람 나이아가라 이펙트 구동
}

void UFT_AladdinFlySkill::OnFlyDurationExpired()
{
    // 지속 시간 만료 즉시 마감 관문 진입
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinFlySkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 안전 해제
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FlyDurationTimerHandle);
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character && Character->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
        
        // 무브먼트 모드 복원 및 속도 원상 복구 (페널티 배율 해제)
        MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
        MoveComp->MaxWalkSpeed /= FlySpeedPenaltyMultiplier;
        
        // 추후 구현 포인트 부착했던 양탄자 메쉬 액터 파괴 및 착지 연출
    }

    // 정상 비행 완료 시점에 15초 공용 유틸기 쿨타임 확정 가동
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}