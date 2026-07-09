// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/FT_AttributeSet.h"

UFT_SilverBulletSkill::UFT_SilverBulletSkill()
    : BaseDamage(50.0f)
    , ChannellingDuration(1.0f)
{
    // 인스턴싱 정책 및 네트워크 실행 정책을 로컬 예측 규격으로 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    // 어빌리티 고유 자산 태그 등록 및 쿨타임 추적용 태그 매핑을 수행합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_SilverBulletSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [자원 커밋 일원화]: 불필요한 중복 검문소를 폐쇄하고 CommitAbility 단일 파이프라인으로 일원화하여 채널링 진입 시점에 자원을 원자적으로 확정합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (SourceASC)
    {
        // 채널링 상태 감시를 위한 루즈 게임플레이 태그를 가동합니다.
        SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }
    
    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        if (WeaponData && WeaponData->AttackMontage)
        {
            UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this, TEXT("SilverBulletChannellingTask"), WeaponData->AttackMontage, 1.0f);

            if (MontageTask)
            {
                // 채널링 도중 피격 캔슬 또는 인터럽트 발생 시 전용 콜백으로 분기합니다.
                MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::HandleChannellingInterrupted);
                MontageTask->ReadyForActivation();
            }
        }
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 채널링 정밀 유지 시간 도달 시 사출을 실행하기 위한 타이머를 타설합니다.
        World->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    ChannellingTimerHandle.Invalidate();

    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid() && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
        UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();

        if (Character && MyASC && Character->GetWeaponData())
        {
            UFT_WeaponData* WeaponData = Character->GetWeaponData();
            UWorld* World = GetWorld();
            
            if (World && WeaponData->ProjectileClass && SilverBulletImpactEffectClass)
            {
                FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                FVector LaunchDirection = Character->GetActorForwardVector();

                // 실전 탄도 동기화 타설: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 포워드 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다.
                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                if (AttributeSet)
                {
                    const float CurrentSpread = AttributeSet->GetWeaponSpread();
                    if (CurrentSpread > 0.0f)
                    {
                        // 평타 및 타 영웅 스킬과 완벽히 호환되는 원형 난수 매핑 방식을 활용해 에임 탄퍼짐 편차를 유도합니다.
                        const float ConeHalfAngleRadius = FMath::DegreesToRadians(CurrentSpread);
                        LaunchDirection = FMath::VRandCone(LaunchDirection, ConeHalfAngleRadius);
                    }
                }

                FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SilverBulletImpactEffectClass, GetAbilityLevel());
                if (SpecHandle.IsValid())
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                }

                // 예측 이중 격발 버그 박멸 완치선: 수동 Apply 사출 구문을 제거하고 오리지널 데이터 핸들을 투사체에 고스란히 봉인 이관합니다.
                // 이로 인해 충돌 오버랩 시점에만 정확히 대미지 판정이 들어가 가상 좀비 릭 및 대미지 씹힘 결함이 완벽히 해결됩니다.
                AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                    WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                
                if (Projectile)
                {
                    // 완벽히 생존 소유권이 보장된 오리지널 스펙 장부를 투사체에 고스란히 이관하여 타깃 적중 시 데미지 증발 현상을 원천 차단합니다.
                    Projectile->DamageEffectSpecHandle = SpecHandle;
                    Projectile->FinishSpawning(SpawnTransform);
                }
            }
        }
    }

    // 사출 완료 시 정상 종료 처리합니다. (bWasCancelled = false 이므로 쿨타임 정상 작동)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::HandleChannellingInterrupted()
{
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

    // 피격 취소 사인을 송신하여 쿨타임 환불 파이프라인 진입을 유도합니다. (bWasCancelled = true)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 메모리 누수 방지: 채널링 정밀 제한 시간 타이머 장부를 청정 소각합니다.
    if (GetWorld() && ChannellingTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
        ChannellingTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 채널링 감시용 루즈 게임플레이 태그를 원점 제거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);

        // [네트워크 동기화 쿨타임 환불]: CC기나 상태이상으로 중간에 캔슬된 경우 우클릭 쿨타임 이펙트를 즉각 회수합니다.
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            // 엔진 공식 순정 함수 매칭 쿼리로 수선하여 무기 스왑이나 채널링 캔슬 시 사격 먹통 결함을 원천 진압합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}