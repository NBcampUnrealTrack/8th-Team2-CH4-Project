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
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

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
   // 좀비 타이머 선제 파쇄 가드선: 새로운 예측 사격 세션이 시작되면 이전 세션의 잔재를 완전히 소각합니다
   if (GetWorld())
   {
       GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
       BurstTimerHandle.Invalidate();
       GetWorld()->GetTimerManager().ClearTimer(NoMontageSafetyTimerHandle);
       NoMontageSafetyTimerHandle.Invalidate();
   }

   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
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

   if (MovementPenaltyGameplayEffectClass)
   {
       FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
       if (PenaltySpecHandle.IsValid())
       {
           MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
       }
   }

   ExecuteWeaponHitDetection(WeaponData, Character);

   if (WeaponData->AttackMontage == nullptr)
   {
       GetWorld()->GetTimerManager().SetTimer(NoMontageSafetyTimerHandle, [this]()
       {
           EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
       }, 0.1f, false);
       
       return;
   }

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

    // 실전 탄도 동기화: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 Forward 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다
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
           bool bIsSameTeam = false;
           if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
           else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;
           
           if (bIsSameTeam)
           {
               EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
               return;
           }
       }

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
        
        // GAS 예측 수명선 마감: 빈 타깃 핸들을 생성하여 생성된 대미지 스펙을 GAS 시스템에 명시적으로 제출, 예측 릭을 완치합니다
        FGameplayAbilityTargetDataHandle EmptyTargetDataHandle;
        ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DamageSpecHandle, EmptyTargetDataHandle);

        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            InWeaponData->ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = DamageSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        return;
    }

    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    TWeakObjectPtr<UFT_NormalAttack> WeakThis(this);

    World->GetTimerManager().SetTimer(BurstTimerHandle, [WeakThis, World, ProjectileClass, Start, Forward, InCharacter, ShotCounter, MaxShots]() mutable
    {
        if (!WeakThis.IsValid() || !WeakThis->IsActive() || !World || !ProjectileClass || *ShotCounter >= MaxShots)
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
            AFTPlayerCharacterBase* PlayerChar = Cast<AFTPlayerCharacterBase>(InCharacter);
            float BaseDamage = (PlayerChar && PlayerChar->GetWeaponData()) ? PlayerChar->GetWeaponData()->BaseDamage : 0.0f;
            DynamicSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        // 점사 내부 예측 데이터 최종 결착: 매 점사 틱마다 생성되는 동적 스펙 역시 정순 제출 배관을 완벽히 타설합니다
        FGameplayAbilityTargetDataHandle DynamicTargetDataHandle;
        WeakThis->ApplyGameplayEffectSpecToTarget(WeakThis->CurrentSpecHandle, WeakThis->CurrentActorInfo, WeakThis->CurrentActivationInfo, DynamicSpecHandle, DynamicTargetDataHandle);

        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
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
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}