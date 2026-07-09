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

    // 사망, 재장전, 기절 상태에서는 기본 공격 격발을 원천 차단합니다.
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
    // 좀비 타이머 선제 파쇄 가드선: 새로운 예측 사격 세션이 시작되면 이전 세션의 잔재 타이머를 완전히 소각합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
        BurstTimerHandle.Invalidate();
        GetWorld()->GetTimerManager().ClearTimer(NoMontageSafetyTimerHandle);
        NoMontageSafetyTimerHandle.Invalidate();
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
    // 원자적 선커밋 검문을 통과시킵니다.
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

    // 사격 시 이동속도 저하 페널티 버프 적용선입니다.
    if (MovementPenaltyGameplayEffectClass)
    {
        FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
        if (PenaltySpecHandle.IsValid())
        {
            MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
        }
    }

    // 데이터 자산에 정의된 발사 유형별 판정 로직을 격발합니다.
    ExecuteWeaponHitDetection(WeaponData, Character);

    // 모션 에셋 누락 시 무한 락에 빠지는 레이스 컨디션을 방어하는 안전 수명선입니다.
    if (WeaponData->AttackMontage == nullptr)
    {
        GetWorld()->GetTimerManager().SetTimer(NoMontageSafetyTimerHandle, [this]()
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }, 0.1f, false);
        
        return;
    }

    // 공격 몽타주 비동기 재생 태스크를 가동합니다.
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
        MontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_NormalAttack::ExecuteWeaponHitDetection(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter) return;

    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60); 
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
    if (!InWeaponData || !InCharacter || !GetWorld()) return;

    // 실전 탄도 동기화: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 Forward 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다.
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
                // 원형 난수 매핑 방식을 활용한 정밀 에임 탄퍼짐 편차 유도
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
            // 팩션 팀 태그 검문을 수행하여 아군 오사 및 팅김 현상을 철저히 예방합니다.
            bool bIsSameTeam = false;
            if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
            else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;
            
            if (bIsSameTeam)
            {
                EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
                return;
            }
        }

        // 부위별 적중 계산에 기반한 헤드샷 대미지 증폭선을 타설합니다.
        float FinalDamage = InWeaponData->BaseDamage;
        if (InWeaponData->bAllowHeadshot)
        {
            if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
            {
                FinalDamage *= InWeaponData->HeadshotMultiplier;
            }
        }

        if (!BaseDamageEffectClass) return;

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
    if (!InWeaponData || !InCharacter || !GetWorld() || !BaseDamageEffectClass) return;

    if (!InWeaponData->ProjectileClass) return;

    UWorld* World = GetWorld();

    // 단발식 투사체 사출 파이프라인 구간입니다.
    if (InWeaponData->ProjectilesPerShot <= 1)
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

        // 예측 이중 격발 버그 박멸 완치선: 수동 Apply 사출 구문을 제거하고 오리지널 데이터 핸들을 투사체에 고스란히 봉인 이관합니다.
        // 이로 인해 충돌 오버랩 시점에만 정확히 대미지 판정이 들어가 가상 좀비 패킷 무효화 및 대미지 씹힘 결함이 완벽히 해결됩니다.
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            InWeaponData->ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            // 완벽히 생존 소유권이 보장된 오리지널 스펙 장부를 투사체에 고스란히 이관하여 타깃 적중 시 데미지 증발 현상을 원천 차단합니다.
            Projectile->DamageEffectSpecHandle = DamageSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        return;
    }

    // 연사 및 점사식 비동기 타이머 기반 투사체 사출 구간입니다.
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    TWeakObjectPtr<UFT_NormalAttack> WeakThis(this);
    TWeakObjectPtr<AFTPlayerCharacterBase> WeakChar(InCharacter);

    World->GetTimerManager().SetTimer(BurstTimerHandle, [WeakThis, WeakChar, World, ProjectileClass, Start, Forward, ShotCounter, MaxShots]() mutable
    {
        // 생존선 정밀 보안 검문: 소유 캐릭터나 어빌리티 본체가 소멸 상태라면 타이머를 정순 강제 파쇄 정지하여 서버 댕글링 Null 크래시를 차단합니다.
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

        // 점사 내부 예측 이중 적용 완치선: 매 연사 틱마다 생성되는 동적 스펙 핸들 역시 수동 Apply 대미지 격발 구문을 제거하여 패킷 크래시를 전면 차단합니다.
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ProjectileClass, SpawnTransform, CharacterPtr, CharacterPtr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            // 소유권이 생존한 오리지널 스펙 장부를 이관하여 점사 투사체의 데미지 증발 현상을 원천 방어합니다.
            Projectile->DamageEffectSpecHandle = DynamicSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        
        (*ShotCounter)++;
        
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
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(BurstTimerHandle))
    {
        return;
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter || !GetWorld() || !BaseDamageEffectClass) return;

    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60);
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
        if (!MyASC) return;

        FGameplayAbilityTargetDataHandle TargetDataHandle;
        FGameplayAbilityTargetData_ActorArray* ActorArrayData = new FGameplayAbilityTargetData_ActorArray();

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();
            if (!HitActor) continue;

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

        // [탄퍼짐 무한 고정 버그 파이프라인 최종 보수 완료]
        // 확정 명세서 매핑: 데이터 자산 내 순정 태생 기저 탄퍼짐 수치인 WeaponData->InitBaseSpread로 정밀 락인합니다.
        if (UFT_AttributeSet* AttributeSet = const_cast<UFT_AttributeSet*>(Cast<UFT_AttributeSet>(SourceASC->GetAttributeSet(UFT_AttributeSet::StaticClass()))))
        {
            AFTPlayerCharacterBase* PlayerChar = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
            
            // 마스터 헤더 장부(InitBaseSpread)와 공차 0.00% 완전 동기화 타설
            float OriginalSpread = (PlayerChar && PlayerChar->GetWeaponData()) ? PlayerChar->GetWeaponData()->InitBaseSpread : 0.0f;
            SourceASC->SetNumericAttributeBase(UFT_AttributeSet::GetWeaponSpreadAttribute(), OriginalSpread);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}