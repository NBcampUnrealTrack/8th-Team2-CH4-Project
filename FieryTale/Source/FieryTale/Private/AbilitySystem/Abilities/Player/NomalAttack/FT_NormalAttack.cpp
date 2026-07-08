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

    // 사망, 재장전, 기절 상태에서는 기본 평타 격발이 완벽히 차단되도록 방어선을 락인합니다.
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

   // 평타 조준 혹은 격발 모션 중 이동 속도 페널티 디버프를 시전자 본인에게 주입하는 구간입니다.
   if (MovementPenaltyGameplayEffectClass)
   {
       FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
       if (PenaltySpecHandle.IsValid())
       {
           MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
       }
   }

   // 무기 에셋의 메커니즘 타입(라인트레이스, 투사체, 근접)을 판별하여 타격을 격발합니다.
   ExecuteWeaponHitDetection(WeaponData, Character);

   // =========================================================================
   // 💡 [애니메이션 부재 완치 배관]: 핸들 중복 오염 데드락을 방어하기 위해 전용 안전 타이머를 가동합니다.
   // 점사형 영웅이 애니메이션 없이 쏠 때도, 탄환 사출 시퀀스를 가로막지 않고 자율 마감을 지원합니다.
   // =========================================================================
   if (WeaponData->AttackMontage == nullptr)
   {
       // 만약 다중 점사 타이머(BurstTimerHandle)가 이미 독립 구동 중이라면, 자율 마감에 일임하고 조기 탈출합니다.
       if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(BurstTimerHandle))
       {
           return;
       }

       // 단발/레이트레이스/근접 영웅인데 애니메이션이 없다면 0.1초 뒤 무결하게 어빌리티 수명 주기를 정상 마감합니다.
       GetWorld()->GetTimerManager().SetTimer(NoMontageSafetyTimerHandle, [this]()
       {
           EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
       }, 0.1f, false);
       
       return;
   }

   // 순정 어빌리티 태스크를 통해 평타 애니메이션 몽타주를 재생하고 완료 시점의 델리게이트를 수신 대기합니다.
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

    // 즉시 발사형(히트스캔) 영웅들을 위한 가시성 채널 단발 레이트레이스 판정 구역입니다.
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
       
       // 팀 진영 태그 비교 검문: 동일 진영(아군 영웅 및 아군 미니언) 타격 시 대미지 주입 연산을 원천 거부합니다.
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

       // 헤드샷 특화 연산: 무기 사양에서 헤드 판정을 허용할 경우 head/neck 본 적중 여부를 대조하여 승수를 가산합니다.
       if (InWeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             FinalDamage *= InWeaponData->HeadshotMultiplier;
          }
       }

       if (!BaseDamageEffectClass) return;

       // 타깃 데이터 핸들을 통해 적중된 컴포넌트 정보와 피격 정보를 포함한 GAS 표준 계산서 패킷을 발송합니다.
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
    // 애니메이션이 끝났더라도 다중 점사 타이머가 여전히 돌고 있다면 안전을 위해 자율 마감 처리를 기다립니다.
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(BurstTimerHandle))
    {
        return;
    }

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
    
    // 복구된 순정 단발/점사 범용 대미지 패킷을 선제 타설합니다.
    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
    if (DamageSpecHandle.IsValid())
    {
        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, InWeaponData->BaseDamage);
    }

    // [단발 사격 분기 복구] 투사체를 1발 사출 후 내부 장부에 대미지 계산서를 이식하여 방출합니다.
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

    // [다중 점사 사격 분기 복구] 정해진 버스트 횟수만큼 지정된 딜레이 간격에 맞추어 투사체를 비동기 연속 사출합니다.
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    // 크래시 완전 방멸을 위한 어빌리티 인스턴스 약참조 세팅
    TWeakObjectPtr<UFT_NormalAttack> WeakThis(this);

    World->GetTimerManager().SetTimer(BurstTimerHandle, [WeakThis, World, ProjectileClass, Start, Forward, InCharacter, DamageSpecHandle, ShotCounter, MaxShots]() mutable
    {
        // 주체가 CC 파쇄 등에 의해 이미 소멸했거나 발사 한계 수치 도달 시 점사 타이머를 클리어합니다.
        if (!WeakThis.IsValid() || !World || !ProjectileClass || *ShotCounter >= MaxShots)
        {
            if (World)
            {
                World->GetTimerManager().ClearTimer(WeakThis->BurstTimerHandle);
                WeakThis->BurstTimerHandle.Invalidate();
            }
            
            if (WeakThis.IsValid())
            {
                WeakThis->EndAbility(WeakThis->CurrentSpecHandle, WeakThis->CurrentActorInfo, WeakThis->CurrentActivationInfo, true, false);
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
        
        // 지정된 모든 탄환 점사 사출 임무 완료 순간 즉시 수명 주기를 정돈 마감합니다.
        if (*ShotCounter >= MaxShots)
        {
            World->GetTimerManager().ClearTimer(WeakThis->BurstTimerHandle);
            WeakThis->BurstTimerHandle.Invalidate();
            WeakThis->EndAbility(WeakThis->CurrentSpecHandle, WeakThis->CurrentActorInfo, WeakThis->CurrentActivationInfo, true, false);
        }
        
    }, Delay, true, 0.0f);
}

void UFT_NormalAttack::PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter)
{
    if (!InWeaponData || !InCharacter || !GetWorld()) return;

    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60);
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    // 근접 영웅 전용 전방 박스 형태의 히트박스 공간 볼륨을 정밀 정의합니다.
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
             
             // 근접 광역 타격 피아식별 필터링
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

                // 내부 참조 카운팅이 지원되는 TSharedPtr 구조의 생성 래퍼를 엮어 메모리 릭을 원천 진압했습니다.
                FGameplayAbilityTargetDataHandle TargetDataHandle;
                TSharedPtr<FGameplayAbilityTargetData_ActorArray> ActorArrayData = MakeShared<FGameplayAbilityTargetData_ActorArray>();
                ActorArrayData->TargetActorArray.Add(HitActor);
                TargetDataHandle.Add(ActorArrayData.Get());

                ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
             }
          }
       }
    }
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 어빌리티 종료 시 가동 중이던 모든 점사 및 안전 타이머 핸들을 완전히 소각 수거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
        BurstTimerHandle.Invalidate();

        GetWorld()->GetTimerManager().ClearTimer(NoMontageSafetyTimerHandle);
        NoMontageSafetyTimerHandle.Invalidate();
    }

    // 공격 종료 타이밍에 맞춰 캐릭터 조준 이동 속도 감산 페널티(GE)를 안전하게 철거 환원합니다.
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