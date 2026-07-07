// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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
       GetWorld()->GetTimerManager().SetTimer(BurstTimerHandle, [this]()
       {
           OnAttackMontageFinished();
       }, 0.2f, false);
       
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

    FVector End = Start + (Forward * InWeaponData->AttackRange);
    
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
        
       UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
       UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
       
       if (MyASC && TargetASC)
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

       FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
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

void UFT_NormalAttack::OnAttackMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::SpawnProjectileLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward)
{
    if (!InWeaponData || !InCharacter || !GetWorld() || !BaseDamageEffectClass) return;

#if !UE_BUILD_SHIPPING
    FVector ArrowEnd = Start + (Forward * 150.f);
    DrawDebugDirectionalArrow(GetWorld(), Start, ArrowEnd, 30.f, FColor::Green, false, 1.5f, 0, 4.0f);
#endif

    if (!InWeaponData->ProjectileClass) return;

    UWorld* World = GetWorld();
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = InCharacter;
    SpawnParams.Instigator = InCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
    if (DamageSpecHandle.IsValid())
    {
        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, InWeaponData->BaseDamage);
    }

    // 단발 사격 분기
    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        FTransform SpawnTransform(Forward.Rotation(), Start);
        
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            InWeaponData->ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = DamageSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        return;
    }

    // 다중 점사 사격 분기
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    World->GetTimerManager().SetTimer(BurstTimerHandle, [this, World, ProjectileClass, Start, Forward, InCharacter, DamageSpecHandle, ShotCounter, MaxShots]() mutable
    {
        if (!World || !ProjectileClass || *ShotCounter >= MaxShots)
        {
            if (World)
            {
                World->GetTimerManager().ClearTimer(BurstTimerHandle);
                BurstTimerHandle.Invalidate();
            }
            return;
        }

        FTransform SpawnTransform(Forward.Rotation(), Start);
        
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = DamageSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        
        (*ShotCounter)++;
        
    }, Delay, true, 0.0f);
}

void UFT_NormalAttack::PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter || !GetWorld()) return;

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

       for (const FOverlapResult& Result : OverlapResults)
       {
          AActor* HitActor = Result.GetActor();
          if (HitActor)
          {
             UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
             if (MyASC && TargetASC)
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

             float FinalDamage = InWeaponData->BaseDamage;

             if (!BaseDamageEffectClass) continue;

             FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
             EffectContext.AddSourceObject(InCharacter);
             FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
             
             if (SpecHandle.IsValid())
             {
                SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);

                FGameplayAbilityTargetDataHandle TargetDataHandle;
                FGameplayAbilityTargetData_ActorArray* ActorArrayData = new FGameplayAbilityTargetData_ActorArray();
                ActorArrayData->TargetActorArray.Add(HitActor);
                TargetDataHandle.Add(ActorArrayData);

                ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
             }
          }
       }
    }
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
        BurstTimerHandle.Invalidate();
    }

    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (MovementPenaltyActiveHandle.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
            MovementPenaltyActiveHandle.Invalidate();
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}