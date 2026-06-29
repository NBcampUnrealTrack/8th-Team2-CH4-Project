// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UFT_NormalAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
   // 스킬을 쓸 수 있는 자원 자원 상태를 검사하고 승인 처리합니다
   if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);    
      return;
   }

   // 캐릭터 본체와 무기 데이터 에셋을 안전하게 로드합니다
   AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
   if (!Character || !Character->GetWeaponData())
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }

   UFT_WeaponData* WeaponData = Character->GetWeaponData();

   // 평타 연동 이동 속도 패널티 연산을 적용합니다
   if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
   {
      // 무기 데이터에 지정된 배율을 현재 최대 걷기 속도에 곱해 일시적으로 감산합니다
      MoveComp->MaxWalkSpeed *= WeaponData->GetMovementPenaltyMultiplier();
   }

   // 애니메이션 몽타주를 재생하는 비동기 태스크를 가동합니다
   UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

   if (MontageTask)
   {
      // 애니메이션이 완료되거나 중단되었을 때 어빌리티를 정리하도록 바인딩합니다
      MontageTask->OnCompleted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      MontageTask->OnInterrupted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      
      // 애니메이션 노티파이를 통해 판정을 제어할 수 있도록 준비합니다
      MontageTask->ReadyForActivation();
   }
   else
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }
}

void UFT_NormalAttack::ExecuteWeaponHitDetection(class UFT_WeaponData* WeaponData, class AFTPlayerCharacterBase* Character)
{
    if (!WeaponData || !Character) return;

    // 시전자의 눈높이 위치와 정면 벡터를 구합니다
    FVector StartLocation = Character->GetActorLocation() + FVector(0, 0, 60); 
    FVector ForwardVector = Character->GetActorForwardVector();

    // 무기 데이터의 발사 타입에 따라 로직을 동적으로 분기합니다
    switch (WeaponData->FireType)
    {
    case EWeaponFireType::LineTrace:
       PerformLineTraceLogic(WeaponData, StartLocation, ForwardVector);
       break;
    case EWeaponFireType::Projectile:
       SpawnProjectileLogic(WeaponData, StartLocation, ForwardVector);
       break;
    case EWeaponFireType::Melee:
       PerformMeleeLogic(WeaponData);
       break;
    }
}

void UFT_NormalAttack::PerformLineTraceLogic(UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    // 사거리를 기반으로 최종 도달점 연산 후 라인트레이스를 구동합니다
    FVector End = Start + (Forward * WeaponData->AttackRange); 
    FHitResult HitResult;
    
    bool bHit = UKismetSystemLibrary::LineTraceSingle(
       GetWorld(), 
       Start, 
       End, 
       ETraceTypeQuery::TraceTypeQuery1,
       false, 
       TArray<AActor*>(), 
       EDrawDebugTrace::ForDuration, 
       HitResult, 
       true
    );

    if (bHit && HitResult.GetActor())
    {
       float FinalDamage = WeaponData->BaseDamage;

       // 헤드샷 허용 무기인 경우 본 이름을 체크하여 데미지 배율을 적용합니다
       if (WeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             FinalDamage *= WeaponData->HeadshotMultiplier;
          }
       }

       // 이펙트 스펙을 생성하여 타깃에게 데미지를 주입합니다
       FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
       if (SpecHandle.IsValid())
       {
          SpecHandle.Data->SetSetByCallerMagnitude(FName("Damage"), FinalDamage);
          
          FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
          
          // 조기 종료 표현식 결과 미사용 경고를 차단하기 위해 void 캐스팅을 적용합니다
          (void)ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
       }
    }
}

void UFT_NormalAttack::OnAttackMontageFinished()
{
    // 공격 행동이 종료되는 시점에 감산되었던 속도를 복구하는 로직을 가동합니다
    if (AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()))
    {
        if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
        {
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                // 패널티 배율의 역수를 곱하여 원래 속도 수치로 안전하게 복원합니다
                float PenaltyMultiplier = WeaponData->GetMovementPenaltyMultiplier();
                if (PenaltyMultiplier > 0.0f)
                {
                    MoveComp->MaxWalkSpeed /= PenaltyMultiplier;
                }
            }
        }
    }

    // 순정 활성화 정보를 매핑하여 안전하게 어빌리티를 정리합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::SpawnProjectileLogic(class UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    if (!WeaponData || !WeaponData->ProjectileClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // 단발성 투사체 또는 연사가 필요 없는 앨리스 카드 형태를 처리합니다
    if (WeaponData->ProjectilesPerShot <= 1)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetAvatarActorFromActorInfo();
        SpawnParams.Instigator = Cast<APawn>(GetAvatarActorFromActorInfo());
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
        
        return;
    }

    // 알라딘 특화 발사 수치 기반 타이머 버스트 연사를 구동합니다
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = WeaponData->ProjectilesPerShot;
    float Delay = WeaponData->BurstDelay;
    UClass* ProjectileClass = WeaponData->ProjectileClass;
    AActor* OwnerActor = GetAvatarActorFromActorInfo();

    FTimerHandle BurstTimerHandle;
    
    World->GetTimerManager().SetTimer(BurstTimerHandle, [World, ProjectileClass, Start, Forward, OwnerActor, ShotCounter, MaxShots]() mutable
    {
        if (*ShotCounter >= MaxShots || !World || !ProjectileClass || !OwnerActor)
        {
            return;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = OwnerActor;
        SpawnParams.Instigator = Cast<APawn>(OwnerActor);
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(ProjectileClass, SpawnTransform, SpawnParams);
        
        (*ShotCounter)++;
        
    }, Delay, true, 0.0f);
}

void UFT_NormalAttack::PerformMeleeLogic(class UFT_WeaponData* WeaponData)
{
    if (!WeaponData) return;

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // 가구야 전방 오버랩 히트박스 연산을 진행합니다
    FVector StartLocation = AvatarActor->GetActorLocation() + FVector(0, 0, 60);
    FVector ForwardVector = AvatarActor->GetActorForwardVector();

    FVector BoxCenter = StartLocation + (ForwardVector * (WeaponData->AttackRange * 0.5f));
    FVector BoxHalfExtent = FVector(WeaponData->AttackRange * 0.5f, 80.0f, 60.0f);
    FQuat BoxRotation = AvatarActor->GetActorQuat();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(AvatarActor);

    TArray<FOverlapResult> OverlapResults;
    
    bool bHit = World->OverlapMultiByChannel(
        OverlapResults,
        BoxCenter,
        BoxRotation,
        ECollisionChannel::ECC_WorldDynamic,
        FCollisionShape::MakeBox(BoxHalfExtent),
        QueryParams
    );

    if (bHit)
    {
       for (const FOverlapResult& Result : OverlapResults)
       {
          AActor* HitActor = Result.GetActor();
          if (HitActor)
          {
             float FinalDamage = WeaponData->BaseDamage;

             FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
             if (SpecHandle.IsValid())
             {
                SpecHandle.Data->SetSetByCallerMagnitude(FName("Damage"), FinalDamage);

                // 안전한 가상 함수 주입 방식으로 타깃 데이터를 생성 및 래핑합니다
                FGameplayAbilityTargetDataHandle TargetDataHandle;
                FGameplayAbilityTargetData_ActorArray* ActorArrayData = new FGameplayAbilityTargetData_ActorArray();
                    
                ActorArrayData->TargetActorArray.Add(HitActor);
                TargetDataHandle.Add(ActorArrayData);

                (void)ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
             }
          }
       }
    }
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}