// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

// [체크 포인트] 관통 은탄 투사체 클래스 헤더 (예시)
// #include "Projectile/FT_SilverBulletProjectile.h"

UFT_SilverBulletSkill::UFT_SilverBulletSkill()
    : BaseDamage(50.0f)
    , ChannellingDuration(1.0f)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    //  우클릭 보조 공격 스킬 쿨다운 태그 매핑
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_SilverBulletSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 12초 쿨다운 자원 검증 및 승인
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    if (SourceASC)
    {
        //  1초 장전 시작 시 "시전 중" 상태 태그를 루즈(Loose) 태그로 부여 (애니메이션 및 이동 제어용)
        SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 1초 장전 타이머 구동
    GetWorld()->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    
    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    // 장전 완료되었으므로 시전 중 채널링 상태 태그 제거
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        UWorld* World = GetWorld();

        if (World && WeaponData->ProjectileClass)
        {
            FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
            FVector LaunchDirection = Character->GetActorForwardVector();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Character;
            SpawnParams.Instigator = Character;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // [기획 반영] 지정 방향으로 넓은 범위를 가진 관통 은탄 사출
            AActor* SpawnedProjectile = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
            
            if (SpawnedProjectile)
            {
                // [기획 반영] 고정 피해량 50 주입 
                // 이 투사체는 적과 부딪혀도 Destroy()되지 않고 관통하며, 적중 시 2초간 50% 둔화(Debuff_Slow)를 부여하도록 블루프린트/클래스 설계
                
                // 예시: AFT_SilverBulletProjectile* Bullet = Cast<AFT_SilverBulletProjectile>(SpawnedProjectile);
                // if(Bullet) { Bullet->ProjectileDamage = BaseDamage; }
            }
        }
    }

    // 발사 완료 후 어빌리티 정리 및 12초 쿨다운 적용 돌입
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 안전 제거
    GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);

    // 만약 시전 도중 끊겼다면(CC기 피격 등) 채널링 태그 확실히 제거
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    if (bWasCancelled && SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 취소되지 않고 정상 발사 완료되었다면 12초 쿨다운 부여 (GE 에셋의 Duration을 12.0으로 세팅)
    if (!bWasCancelled)
    {
        (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}