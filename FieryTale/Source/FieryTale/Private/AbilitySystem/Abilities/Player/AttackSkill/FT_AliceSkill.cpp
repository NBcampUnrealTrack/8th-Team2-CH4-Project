// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h" 
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_AliceSkill::UFT_AliceSkill()
    : BaseDamage(30.0f) // 기획 스펙: 관통 피해량 30
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    // [RMB] 보조 공격 공용 재사용 대기시간 10초 매핑
    CooldownTag = FTTags::FTStates::Cooldown_RightClick;
}

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 10초 쿨다운 및 자원 검증
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        UWorld* World = GetWorld();

        if (World && WeaponData->ProjectileClass)
        {
            // 캐릭터 전방에서 회중시계 토끼 환영 투사체 생성
            FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
            FVector LaunchDirection = Character->GetActorForwardVector();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Character;
            SpawnParams.Instigator = Character;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // 직선으로 빠르게 달리는 회중시계 토끼 환영 액터 사출
            AActor* SpawnedRabbit = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
            
            if (SpawnedRabbit)
            {
                // [기획 구현 가이드] 사출된 토끼 투사체 내부에 앨리스의 고정 피해(30) 설정 전달
                // 이 투사체는 충돌 시 Destroy되지 않고 Overlap 채널을 통해 적들을 '관통'하며,
                // 적중된 대상의 ASC에 Damage(30), Debuff_Slow(2초), Target_Insignia(태엽 인장) GE를 찌르도록 블루프린트와 연동됩니다.
            }
        }
    }

    // 환영 사출 직후 스킬 즉시 종료 및 쿨다운 돌입
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}