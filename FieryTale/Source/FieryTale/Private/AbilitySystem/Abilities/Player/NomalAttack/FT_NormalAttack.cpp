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

UFT_NormalAttack::UFT_NormalAttack()
    : OriginalMaxWalkSpeed(0.0f)
{
    // [인스턴싱 정책] 영웅들마다 독립적인 평타 연사 속도 및 점사 타이머를 추적하기 위해 액터당 독립 인스턴스로 제어합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [네트워크 정책] 멀티플레이 레이턴시를 지우고 누르는 즉시 사격 피드백을 느끼도록 클라이언트 선입력 예측을 기동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [하드 조작 차단벽] 시전자 본체 상태 태그 명세를 검사하여 행동 불가 상태일 경우 평타 발동을 원천 통제합니다.
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

    // 시전자 캐스팅 안전 검증 및 무기 데이터 에셋 존재 여부를 선제 필터링합니다.
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
    
   // 글로벌 쿨다운 및 침묵 상태 이상 여부를 가동 전에 검증합니다.
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

   // [이속 감산 구현] 사격 중 이동 속도 패널티 연산 전 순정 최대 속도를 안전하게 백업한 뒤 고유 배율을 곱합니다.
   if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
   {
      OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
      MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * WeaponData->GetMovementPenaltyMultiplier();
   }

   // 1. 데이터 에셋 설정(FireType)에 따라 3대 물리 히트 스캔 라인을 즉시 가동합니다.
   ExecuteWeaponHitDetection(WeaponData, Character);

   // 2. [시차 릭 완전 해제] 아트 리소스가 없어 몽타주가 None일 때 동일 프레임에 어빌리티가 바로 끝나면
   // 엔진이 DrawDebug 패킷을 그리기도 전에 월드 주소를 떼어버려 드로우가 유실됩니다.
   // 0.2초의 최소 시각화 보장 타이머를 열어 렌더링 프레임 버퍼를 안전하게 확보하고 탈출시킵니다.
   if (WeaponData->AttackMontage == nullptr)
   {
       GetWorld()->GetTimerManager().SetTimer(BurstTimerHandle, [this]()
       {
           OnAttackMontageFinished();
       }, 0.2f, false);
       
       return;
   }

   // [정석 비비기] 애니메이션 몽타주 재생 및 인터럽트 감시 비동기 태스크 가동
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

    // [좌표 보정] 스켈레탈 메쉬가 없는 깡통 상태에서도 캡슐 충돌체 간섭 없이 레이저가 안전하게 뻗도록 피벗 기준 눈높이 버퍼만 조립합니다.
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

    // [하늘 사격 예외 케이스] 바닥 맵 콜라이더 릭 간섭을 원천 차단하기 위해 순정 UpVector 방향으로 쏘아올립니다.
    FVector UpVector = InCharacter->GetActorUpVector();
    FVector End = Start + (UpVector * InWeaponData->AttackRange); 
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter); 
    QueryParams.bReturnPhysicalMaterial = false;

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, QueryParams);

#if !UE_BUILD_SHIPPING
    // [시각화 오버로딩] 마지막 인자에 두께(5.0f)와 잔상 유지 시간(4.0f)을 강제 주입해 묵직한 레이저 기둥을 보장합니다.
    DrawDebugLine(GetWorld(), Start, bHit ? HitResult.ImpactPoint : End, FColor::Red, false, 2.0f, 0, 5.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 30.f, 12, FColor::Yellow, false, 3.0f, 0, 2.0f);
    }
#endif

    // [대미지 우체통 파이프라인] 피격 대상이 존재할 때 최종 데미지 연산 후 타겟 ASC에 사출합니다.
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
    if (!InWeaponData || !InCharacter || !GetWorld()) return;

#if !UE_BUILD_SHIPPING
    // 메쉬가 없는 깡통 상태에서도 탄환 배방 방위를 직관적으로 감시하도록 전방에 가상 드로우 화살표를 투사합니다.
    FVector ArrowEnd = Start + (Forward * 150.f);
    DrawDebugDirectionalArrow(GetWorld(), Start, ArrowEnd, 30.f, FColor::Green, false, 1.5f, 0, 4.0f);
#endif

    if (!InWeaponData->ProjectileClass)
    {
        UE_LOG(LogTemp, Log, TEXT("[NormalAttack] 테스트 모드: ProjectileClass가 없지만 가상 사출 화살표 드로우를 성립시켰습니다."));
        return;
    }

    UWorld* World = GetWorld();
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = InCharacter;
    SpawnParams.Instigator = InCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(InWeaponData->ProjectileClass, SpawnTransform, SpawnParams);
        return;
    }

    // [점사 인프라 개통] 발당 탄환 개수 수치가 높게 세팅된 경우 버스트 딜레이 반복 타이머를 전개합니다.
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    World->GetTimerManager().SetTimer(BurstTimerHandle, [World, ProjectileClass, Start, Forward, SpawnParams, ShotCounter, MaxShots]() mutable
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

    // [근접 판정 조립] 캐릭터 피벗 정면 리치 반경만큼 전방으로 확장된 다중 충돌 박스 볼륨을 생성합니다.
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
    // 강타 범위를 청록색(Cyan)으로 그리며, 타격 포착 즉시 보라색(Magenta)으로 점멸 반전 피드백을 구현합니다.
    FColor DebugBoxColor = bHit ? FColor::Magenta : FColor::Cyan;
    DrawDebugBox(GetWorld(), BoxCenter, BoxHalfExtent, BoxRotation, DebugBoxColor, false, 1.5f, 0, 2.5f);
#endif

    if (bHit)
    {
       for (const FOverlapResult& Result : OverlapResults)
       {
          AActor* HitActor = Result.GetActor();
          if (HitActor)
          {
             float FinalDamage = InWeaponData->BaseDamage;

             if (!BaseDamageEffectClass) continue;

             FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
             EffectContext.AddSourceObject(InCharacter);
             FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
             
             if (SpecHandle.IsValid())
             {
                SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);

                // [GC 크래시 방어 완착] 원시 포인터 수동 new 할당으로 인한 가비지 릭을 완벽 철거하고
                // 핸들이 스레드 소유권을 직접 양도받아 안전 수거하도록 정석 아키텍처로 빌드 매핑합니다.
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
    // [인터럽트 방어] 하드 CC나 스킬 캔슬을 맞더라도 백그라운드 점사 타이머 잔재를 완전히 소멸시켜 탄환 누수를 완벽 차단합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
    }

    // [이속 복구 확정] 공격 상태 탈출 시 처음 백업해 두었던 순정 수치 그대로 이동 속도를 안전 회귀시킵니다.
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
            {
                if (OriginalMaxWalkSpeed > 0.0f)
                {
                    MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}