// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    // 인스턴싱 및 네트워크 실행 정책을 로컬 예측 규격으로 설정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 특정 상태에서는 활성화를 차단합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Core::Dead);
    ActivationBlockedTags.AddTag(FTTags::FTStates::Core::Reloading);
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

bool UFT_NormalAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    AFTPlayerCharacterBase* PlayerChar = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!PlayerChar || !PlayerChar->GetWeaponData())
    {
        return false;
    }

    return true;
}

void UFT_NormalAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 이전 타이머를 초기화합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
        BurstTimerHandle.Invalidate();
        GetWorld()->GetTimerManager().ClearTimer(NoMontageSafetyTimerHandle);
        NoMontageSafetyTimerHandle.Invalidate();
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
    // 어빌리티 활성화 조건 및 코스트를 검증합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);    
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!Character || !Character->GetWeaponData() || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UFT_WeaponData* WeaponData = Character->GetWeaponData();

    // 이동 속도 감소 이펙트를 적용합니다.
    if (MovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid() && PenaltySpecHandle.Data.IsValid())
        {
            // 웨폰 데이터의 이동 속도 배율 값을 그대로 적용합니다. (0이면 이동 불가)
            float SpeedMulti = WeaponData->MovementSpeedMultiplier;
            PenaltySpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::MovementPenalty, SpeedMulti);
            
            MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }

    // 무기 타입에 따른 공격 판정을 실행합니다.
    ExecuteWeaponHitDetection(WeaponData, Character);

    // 몽타주가 없을 경우 안전을 위해 지연 후 종료합니다.
    if (WeaponData->AttackMontage == nullptr)
    {
        GetWorld()->GetTimerManager().SetTimer(NoMontageSafetyTimerHandle, [this]()
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }, 0.1f, false);
        
        return;
    }

    // 몽타주 재생 태스크 실행 및 콜백 등록
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
        
        MontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_NormalAttack::ExecuteWeaponHitDetection(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter)
    {
        return;
    }
    FVector StartLocation = InCharacter->GetWeaponMuzzleLocation();
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    switch (InWeaponData->FireType)
    {
    case EWeaponFireType::LineTrace:
        PerformLineTraceLogic(InWeaponData, InCharacter, StartLocation, ForwardVector);
        break;
    case EWeaponFireType::Projectile:
        SpawnProjectileLogic(InWeaponData, InCharacter, StartLocation, ForwardVector);
        break;
    case EWeaponFireType::Melee:
        PerformMeleeLogic(InWeaponData, InCharacter);
        break;
    }
}

void UFT_NormalAttack::PerformLineTraceLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward)
{
    if (!InWeaponData || !InCharacter || !GetWorld())
    {
        return;
    }
    
    // 탄퍼짐 적용
    FVector FinalForward = Forward;
    UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
    if (MyASC)
    {
        const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
        if (AttributeSet)
        {
            const float CurrentSpread = AttributeSet->GetWeaponSpread();
            if (CurrentSpread > 0.0f)
            {

                const float ConeHalfAngleRadius = FMath::DegreesToRadians(CurrentSpread);
                FinalForward = FMath::VRandCone(Forward, ConeHalfAngleRadius);
            }
        }
    }

    FVector End = Start + (FinalForward * InWeaponData->AttackRange);
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter);
    QueryParams.bReturnPhysicalMaterial = false;

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, QueryParams);

#if !UE_BUILD_SHIPPING
    DrawDebugLine(GetWorld(), Start, bHit ? HitResult.ImpactPoint : End, FColor::Red, false, 2.0f, 0, 5.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 30.f, 12, FColor::Yellow, false, 3.0f, 0, 2.0f);
    }
#endif

    if (bHit && HitResult.GetActor())
    {
        AActor* HitActor = HitResult.GetActor();
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
        
        if (MyASC && TargetASC)
        {
            // 피아 식별 검증
            bool bIsSameTeam = false;
            if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
            else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;
            

            if (bIsSameTeam)
            {
                return;
            }
        }

        // 헤드샷 대미지 적용
        float FinalDamage = InWeaponData->BaseDamage;
        if (InWeaponData->bAllowHeadshot)
        {
            if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
            {
                FinalDamage *= InWeaponData->HeadshotMultiplier;
            }
        }

        if (!BaseDamageEffectClass)
        {
            return;
        }
        
        FGameplayEffectContextHandle EffectContext = MyASC->MakeEffectContext();
        EffectContext.AddSourceObject(InCharacter);
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
        
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);
            FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
            ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
        }
    }
}

void UFT_NormalAttack::SpawnProjectileLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward)
{
    if (!InWeaponData || !InCharacter || !GetWorld() || !BaseDamageEffectClass)
    {
        return;
    }
    
    if (!InWeaponData->ProjectileClass)
    {
        return;
    }
    UWorld* World = GetWorld();

    // 단발 투사체 스폰
    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        // 서버에서만 투사체를 스폰합니다.
        if (InCharacter->HasAuthority())
        {
            FVector FinalForward = Forward;
            if (UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo())
            {
                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                if (AttributeSet && AttributeSet->GetWeaponSpread() > 0.0f)
                {
                    FinalForward = FMath::VRandCone(Forward, FMath::DegreesToRadians(AttributeSet->GetWeaponSpread()));
                }
            }

#if !UE_BUILD_SHIPPING
            FVector ArrowEnd = Start + (FinalForward * 150.f);
            DrawDebugDirectionalArrow(World, Start, ArrowEnd, 30.f, FColor::Green, false, 1.5f, 0, 4.0f);
#endif

            FTransform SpawnTransform(FinalForward.Rotation(), Start);
            FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
            if (DamageSpecHandle.IsValid())
            {
                DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, InWeaponData->BaseDamage);
            }

            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                InWeaponData->ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                Projectile->DamageEffectSpecHandle = DamageSpecHandle;
                Projectile->FinishSpawning(SpawnTransform);
            }
        }
        return;
    }

    // 점사 타이머 설정
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    TWeakObjectPtr<UFT_NormalAttack> WeakThis(this);
    TWeakObjectPtr<AFTPlayerCharacterBase> WeakChar(InCharacter);

    World->GetTimerManager().SetTimer(BurstTimerHandle, [WeakThis, WeakChar, World, ProjectileClass, Start, Forward, ShotCounter, MaxShots]() mutable
    {
        // 유효성 검증
        if (!WeakThis.IsValid() || !WeakThis->IsActive() || !WeakChar.IsValid() || !World || !ProjectileClass || *ShotCounter >= MaxShots)
        {
            if (World && WeakThis.IsValid())
            {
                World->GetTimerManager().ClearTimer(WeakThis->BurstTimerHandle);
                WeakThis->BurstTimerHandle.Invalidate();
                
                if (WeakThis->IsActive())
                {
                    WeakThis->EndAbility(WeakThis->CurrentSpecHandle, WeakThis->CurrentActorInfo, WeakThis->CurrentActivationInfo, true, false);
                }
            }
            return;
        }

        AFTPlayerCharacterBase* CharacterPtr = WeakChar.Get();
        
        // 서버에서 투사체 스폰을 처리합니다.
        if (CharacterPtr->HasAuthority())
        {
            FVector FinalForward = Forward;
            UAbilitySystemComponent* MyASC = WeakThis->GetAbilitySystemComponentFromActorInfo();
            if (MyASC)
            {
                const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                if (AttributeSet && AttributeSet->GetWeaponSpread() > 0.0f)
                {
                    FinalForward = FMath::VRandCone(Forward, FMath::DegreesToRadians(AttributeSet->GetWeaponSpread()));
                }
            }

#if !UE_BUILD_SHIPPING
            FVector ArrowEnd = Start + (FinalForward * 150.f);
            DrawDebugDirectionalArrow(World, Start, ArrowEnd, 30.f, FColor::Green, false, 1.5f, 0, 4.0f);
#endif

            FTransform SpawnTransform(FinalForward.Rotation(), Start);
            
            FGameplayEffectSpecHandle DynamicSpecHandle = WeakThis->MakeOutgoingGameplayEffectSpec(WeakThis->BaseDamageEffectClass, WeakThis->GetAbilityLevel());
            
            if (DynamicSpecHandle.IsValid())
            {
                float BaseDamage = CharacterPtr->GetWeaponData() ? CharacterPtr->GetWeaponData()->BaseDamage : 0.0f;
                DynamicSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
            }

            AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                ProjectileClass, SpawnTransform, CharacterPtr, CharacterPtr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            
            if (Projectile)
            {
                Projectile->DamageEffectSpecHandle = DynamicSpecHandle;
                Projectile->FinishSpawning(SpawnTransform);
            }
        }
        
        (*ShotCounter)++;
        
        // 점사가 완료되면 타이머를 해제하고 어빌리티를 종료합니다.
        if (*ShotCounter >= MaxShots)
        {
            World->GetTimerManager().ClearTimer(WeakThis->BurstTimerHandle);
            WeakThis->BurstTimerHandle.Invalidate();
            
            if (WeakThis->IsActive())
            {
                WeakThis->EndAbility(WeakThis->CurrentSpecHandle, WeakThis->CurrentActorInfo, WeakThis->CurrentActivationInfo, true, false);
            }
        }
        
    }, Delay, true, 0.0f);
}

void UFT_NormalAttack::OnAttackMontageFinished()
{
    // 점사 타이머가 활성화되어 있다면 종료를 대기합니다.
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(BurstTimerHandle))
    {
        return;
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter || !GetWorld() || !BaseDamageEffectClass)
    {
        return;
    }
    FVector StartLocation = InCharacter->GetWeaponMuzzleLocation();
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    FVector BoxCenter = StartLocation + (ForwardVector * (InWeaponData->AttackRange * 0.5f));
    FVector BoxHalfExtent = FVector(InWeaponData->AttackRange * 0.5f, 80.0f, 60.0f);
    FQuat BoxRotation = InCharacter->GetActorQuat();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter);

    TArray<FOverlapResult> OverlapResults;
    bool bHit = GetWorld()->OverlapMultiByChannel(OverlapResults, BoxCenter, BoxRotation, ECollisionChannel::ECC_WorldDynamic, FCollisionShape::MakeBox(BoxHalfExtent), QueryParams);

#if !UE_BUILD_SHIPPING
    FColor DebugBoxColor = bHit ? FColor::Magenta : FColor::Cyan;
    DrawDebugBox(GetWorld(), BoxCenter, BoxHalfExtent, BoxRotation, DebugBoxColor, false, 1.5f, 0, 2.5f);
#endif

    if (bHit)
    {
        UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
        if (!MyASC)
        {
            return;
        }
        FGameplayAbilityTargetDataHandle TargetDataHandle;
        FGameplayAbilityTargetData_ActorArray* ActorArrayData = new FGameplayAbilityTargetData_ActorArray();

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();
            if (!HitActor)
            {
                continue;
            }
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            
            if (TargetASC)
            {
                bool bIsSameTeam = false;
                
                if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && 
                    TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
                {
                    bIsSameTeam = true;
                }
                else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && 
                         TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
                {
                    bIsSameTeam = true;
                }
                
                if (bIsSameTeam)
                {
                    continue;
                }
            }

            ActorArrayData->TargetActorArray.Add(HitActor);
        }

        if (ActorArrayData->TargetActorArray.Num() > 0)
        {
            TargetDataHandle.Add(ActorArrayData);

            FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
            if (SpecHandle.IsValid())
            {
                SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, InWeaponData->BaseDamage);
                ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
            }
            else
            {
                delete ActorArrayData;
            }
        }
        else
        {
            delete ActorArrayData;
        }
    }
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
        BurstTimerHandle.Invalidate();

        GetWorld()->GetTimerManager().ClearTimer(NoMontageSafetyTimerHandle);
        NoMontageSafetyTimerHandle.Invalidate();
    }

    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

        if (MovementPenaltyActiveHandle.IsValid())
        {
            SourceASC->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
            MovementPenaltyActiveHandle.Invalidate();
        }

        // 탄퍼짐 속성을 원래 값으로 복구합니다.
        if (UFT_AttributeSet* AttributeSet = const_cast<UFT_AttributeSet*>(Cast<UFT_AttributeSet>(SourceASC->GetAttributeSet(UFT_AttributeSet::StaticClass()))))
        {
            AFTPlayerCharacterBase* PlayerChar = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
            
            float OriginalSpread = (PlayerChar && PlayerChar->GetWeaponData()) ? PlayerChar->GetWeaponData()->InitBaseSpread : 0.0f;
            SourceASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), OriginalSpread);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}