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
    : MaxDuration(4.0f)
    , SpawnedShieldActor(nullptr)
{
    // 인스턴싱 및 네트워크 실행 정책을 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 자산 태그 및 쿨타임 태그를 설정합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;

    // 활성화 시 반격 대기 상태 태그를 부여합니다.
    ActivationOwnedTags.AddTag(FTTags::FTStates::Buff::CounterReady);
}

void UFT_KaguyaBulwarkAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 어빌리티 활성화 조건 및 코스트를 검증합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!Character || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 채널링 상태 태그를 부여합니다.
    SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    
    // 이동 속도 감소 이펙트를 적용합니다.
    if (MovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }

    // 몽타주 재생 태스크 실행
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    UAnimMontage* LoadedMontage = WeaponData ? WeaponData->AttackMontage.LoadSynchronous() : nullptr;
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CounterGuardTask"), LoadedMontage, 1.0f);

        if (MontageTask)
        {
            // 인터럽트 콜백 등록
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_KaguyaBulwarkAbility::OnGuardInterrupted);
            MontageTask->ReadyForActivation();
        }
    }

    // 방벽 지속 타이머 설정
    if (GetWorld())
    {
        // 방패 에셋 비동기(또는 동기) 로드
        UClass* LoadedShieldClass = BulwarkShieldClass.LoadSynchronous();

        // 1P 서버 권한 확인 후 거대 방패 액터 스폰
        if (Character->HasAuthority() && LoadedShieldClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Character;
            SpawnParams.Instigator = Character;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // 캐릭터 정면 방향으로 살짝 앞쪽에 배치
            FVector SpawnLocation = Character->GetActorLocation() + (Character->GetActorForwardVector() * 100.0f);
            FRotator SpawnRotation = Character->GetActorRotation();

            SpawnedShieldActor = GetWorld()->SpawnActor<AActor>(LoadedShieldClass, SpawnLocation, SpawnRotation, SpawnParams);
            
            if (SpawnedShieldActor)
            {
                // 캐릭터에 부착하여 이동/회전을 따라가도록 처리
                SpawnedShieldActor->AttachToActor(Character, FAttachmentTransformRules::KeepWorldTransform);
            }
        }

        // 타 스킬/평타 사용 시 방벽 해제를 위한 델리게이트 리스너 등록
        AbilityActivatedHandle = SourceASC->AbilityActivatedCallbacks.AddUObject(this, &UFT_KaguyaBulwarkAbility::OnAnyAbilityActivated);

        GetWorld()->GetTimerManager().SetTimer(
            BulwarkDurationTimerHandle, 
            this, 
            &UFT_KaguyaBulwarkAbility::ExpireBulwark, 
            MaxDuration, 
            false
        );
    }
}

void UFT_KaguyaBulwarkAbility::OnAnyAbilityActivated(UGameplayAbility* ActivatedAbility)
{
    // 자기 자신(방벽 스킬)이 아닌 다른 스킬이나 평타가 발동되었을 때
    if (ActivatedAbility && ActivatedAbility != this)
    {
        // 평타 또는 스킬 태그를 가진 어빌리티인지 확인
        if (ActivatedAbility->GetAssetTags().HasTag(FTTags::FTAbilities::AttackSkill) ||
            ActivatedAbility->GetAssetTags().HasTag(FTTags::FTAbilities::NormalAttack) ||
            ActivatedAbility->GetAssetTags().HasTag(FTTags::FTAbilities::UtilSkill) ||
            ActivatedAbility->GetAssetTags().HasTag(FTTags::FTAbilities::UltimateSkill))
        {
            // 방벽 즉시 해제 (강제 취소)
            CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        }
    }
}

void UFT_KaguyaBulwarkAbility::ExpireBulwark()
{
    // 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_KaguyaBulwarkAbility::OnGuardInterrupted()
{
    // 어빌리티가 강제 취소되었을 때 처리
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_KaguyaBulwarkAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 해제
    if (GetWorld() && BulwarkDurationTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(BulwarkDurationTimerHandle);
        BulwarkDurationTimerHandle.Invalidate();
    }

    // 방패 액터 파괴
    if (SpawnedShieldActor)
    {
        SpawnedShieldActor->Destroy();
        SpawnedShieldActor = nullptr;
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 델리게이트 리스너 해제
        if (AbilityActivatedHandle.IsValid())
        {
            SourceASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
            AbilityActivatedHandle.Reset();
        }
        // 채널링 태그 및 이동 속도 감소 이펙트 제거
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
        
        if (MovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
            MovementPenaltyActiveHandle.Invalidate();
        }

        // 어빌리티가 취소되었을 경우 쿨타임 초기화
        if (bWasCancelled)
        {
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            // 해당 쿨타임 태그를 가진 이펙트를 제거합니다.
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            SourceASC->RemoveActiveEffects(CooldownQuery);
        }
    }
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}