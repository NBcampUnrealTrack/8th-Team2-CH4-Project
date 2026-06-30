// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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
   if (!Character || !Character->GetWeaponData())
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }

   UFT_WeaponData* WeaponData = Character->GetWeaponData();

   if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
   {
      MoveComp->MaxWalkSpeed *= WeaponData->GetMovementPenaltyMultiplier();
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
      return;
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

    bool bHit = GetWorld()->LineTraceSingleByChannel(
       HitResult, 
       Start, 
       End, 
       ECollisionChannel::ECC_Visibility, 
       QueryParams
    );

#if !UE_BUILD_SHIPPING
    DrawDebugLine(GetWorld(), Start, bHit ? HitResult.ImpactPoint : End, FColor::Red, false, 1.0f, 0, 2.0f);
    if (bHit)
    {
        DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.f, FColor::Red, false, 1.0f);
    }
#endif

    if (bHit && HitResult.GetActor())
    {
       float FinalDamage = InWeaponData->BaseDamage;

       if (InWeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             FinalDamage *= InWeaponData->HeadshotMultiplier;
          }
       }

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
    if (AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()))
    {
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                float PenaltyMultiplier = WeaponData->GetMovementPenaltyMultiplier();
                if (PenaltyMultiplier > 0.0f)
                {
                    MoveComp->MaxWalkSpeed /= PenaltyMultiplier;
                }
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::SpawnProjectileLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward)
{
    if (!InWeaponData || !InWeaponData->ProjectileClass || !InCharacter) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = InCharacter;
    SpawnParams.Instigator = InCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    float BaseDmg = InWeaponData->BaseDamage;

    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(InWeaponData->ProjectileClass, SpawnTransform, SpawnParams);
        return;
    }

    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    FTimerHandle BurstTimerHandle;
    
    World->GetTimerManager().SetTimer(BurstTimerHandle, [World, ProjectileClass, Start, Forward, SpawnParams, ShotCounter, MaxShots, BaseDmg]() mutable
    {
        if (*ShotCounter >= MaxShots || !World || !ProjectileClass)
        {
            return;
        }

        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(ProjectileClass, SpawnTransform, SpawnParams);
        
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
    
    bool bHit = GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        BoxCenter,
        BoxRotation,
        ECollisionChannel::ECC_WorldDynamic,
        FCollisionShape::MakeBox(BoxHalfExtent),
        QueryParams
    );

#if !UE_BUILD_SHIPPING
    DrawDebugBox(GetWorld(), BoxCenter, BoxHalfExtent, BoxRotation, FColor::Red, false, 1.0f, 0, 2.0f);
#endif

    if (bHit)
    {
       for (const FOverlapResult& Result : OverlapResults)
       {
          AActor* HitActor = Result.GetActor();
          if (HitActor)
          {
             float FinalDamage = InWeaponData->BaseDamage;

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
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}