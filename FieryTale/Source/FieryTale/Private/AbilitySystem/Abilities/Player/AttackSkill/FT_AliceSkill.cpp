// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h" 
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameplayTags/FTTags.h"

UFT_AliceSkill::UFT_AliceSkill()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Buff.HackingMode")));
}

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    TrackedTargets.Empty();

    GetWorld()->GetTimerManager().SetTimer(LockOnTimerHandle, this, &UFT_AliceSkill::ScanHackingTargets, 0.2f, true);
}

void UFT_AliceSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    GetWorld()->GetTimerManager().ClearTimer(LockOnTimerHandle);

    if (!bWasCancelled)
    {
       (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);

       AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
       if (Character && Character->GetWeaponData())
       {
           UFT_WeaponData* WeaponData = Character->GetWeaponData();
           UWorld* World = GetWorld();

           if (World && WeaponData->ProjectileClass)
           {
               FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
               
               FActorSpawnParameters SpawnParams;
               SpawnParams.Owner = Character;
               SpawnParams.Instigator = Character;
               SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

               for (AActor* TargetActor : TrackedTargets)
               {
                   if (IsValid(TargetActor))
                   {
                       FVector Direction = (TargetActor->GetActorLocation() - SpawnLocation).GetSafeNormal();
                       FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);

                       World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
                   }
               }
           }
       }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFT_AliceSkill::ScanHackingTargets()
{
    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
    if (!Character || !Character->GetWeaponData()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector StartLocation = Character->GetActorLocation() + FVector(0, 0, 60);
    FVector ForwardVector = Character->GetActorForwardVector();
    
    float ScanRange = Character->GetWeaponData()->AttackRange;
    FVector EndLocation = StartLocation + (ForwardVector * ScanRange);

    TArray<FHitResult> HitResults;
    FCollisionShape ScanShape = FCollisionShape::MakeSphere(250.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);

    bool bHit = World->SweepMultiByChannel(
        HitResults,
        StartLocation,
        EndLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_WorldDynamic,
        ScanShape,
        QueryParams
    );

    if (bHit)
    {
        UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
        if (!SourceASC) return;

        FGameplayTag BlueTeamTag = FGameplayTag::RequestGameplayTag(FName("Team.Blue"));
        FGameplayTag RedTeamTag = FGameplayTag::RequestGameplayTag(FName("Team.Red"));

        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (IsValid(HitActor))
            {
                // 정석적인 언리얼 인터페이스 캐스팅 방식을 적용하여 안전하게 검증합니다
                if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
                {
                    if (UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent())
                    {
                        if ((SourceASC->HasMatchingGameplayTag(BlueTeamTag) && TargetASC->HasMatchingGameplayTag(BlueTeamTag)) ||
                            (SourceASC->HasMatchingGameplayTag(RedTeamTag) && TargetASC->HasMatchingGameplayTag(RedTeamTag)))
                        {
                            continue;
                        }

                        if (!TrackedTargets.Contains(HitActor))
                        {
                            TrackedTargets.Add(HitActor);
                        }
                    }
                }
            }
        }
    }
}