// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UtillSkill/FT_AladdinFlySkill.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinFlySkill::UFT_AladdinFlySkill()
    : FlyDuration(1.5f)
    , FlySpeedBoost(400.0f)
    , OrigWalkSpeed(0.0f)
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Shift 유틸기 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UtillSkill);
    SetAssetTags(AssetTags);

    // 비행 상태 동안 일시적으로 공중 비행 상태 태그를 캐릭터에게 부여합니다
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
    if (!Character || !Character->GetCharacterMovement())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();

    // 원본 속도를 기억하고 양탄자 고속 비행을 위해 무브먼트 모드를 전환 및 가속합니다
    OrigWalkSpeed = MoveComp->MaxWalkSpeed;
    MoveComp->MaxFlySpeed = OrigWalkSpeed + FlySpeedBoost;
    MoveComp->MaxWalkSpeed = MoveComp->MaxFlySpeed;
    
    // 지면 마찰과 중력의 영향을 끄고 비행 모드로 강제 설정합니다
    MoveComp->SetMovementMode(EMovementMode::MOVE_Flying);

    // 공중으로 살짝 떠오르는 부력을 인위적으로 한 번 튕겨줍니다
    Character->LaunchCharacter(FVector(0.0f, 0.0f, 250.0f), false, true);

    // 기획 스펙 지정된 비행 지속 시간 타이머를 구동합니다
    GetWorld()->GetTimerManager().SetTimer(FlyTimerHandle, this, &UFT_AladdinFlySkill::OnFlyDurationExpired, FlyDuration, false);

    // 추후 구현 포인트 비행 시작 시 스폰할 양탄자 스켈레탈 메쉬 컴포넌트 부착 및 기류 바람 나이아가라 이펙트 구동 구역
}

void UFT_AladdinFlySkill::OnFlyDurationExpired()
{
    // 지속 시간이 다 닳는 즉시 어빌리티를 정리하는 관문으로 안전하게 진입합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinFlySkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머가 돌고 있다면 강제로 해제하여 좀도둑 크래시를 방지합니다
    GetWorld()->GetTimerManager().ClearTimer(FlyTimerHandle);

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character && Character->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
        
        // 비행이 끝났으므로 원래 정통 걷기 무브먼트 모드로 완벽하게 복원합니다
        MoveComp->SetMovementMode(EMovementMode::MOVE_Walking);
        
        if (OrigWalkSpeed > 0.0f)
        {
            MoveComp->MaxWalkSpeed = OrigWalkSpeed;
        }
        
        // 추후 구현 포인트 부착했던 양탄자 메쉬 액터 파괴 및 지면 착지 애니메이션 연동 구역
    }

    if (!bWasCancelled)
    {
        // 정상 비행 완료 시점에 쿨타임을 정상 가동합니다
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}