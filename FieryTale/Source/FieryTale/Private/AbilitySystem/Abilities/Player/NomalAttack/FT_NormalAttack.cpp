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
#include"Object/FT_ProjectileBase.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    // 인스턴싱 정책: 공격 주기 및 타이머 연산 데이터 격리를 위해 캐릭터당 독립 인스턴스화
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 평타 입력 반응성을 위해 로컬 선예측 후 서버 검증 규칙 채택
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [평타 시전 봉쇄 태그 설정]
    // 사망 상태, 장전 상태, 기절 상태이상이 걸려있을 경우 평타 격발을 엔진 단에서 원천 차단합니다.
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

    // 아바타 액터가 정상적인 플레이어인지, 무기 데이터 에셋이 안전하게 로드되어 있는지 사전 장부 검수
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
    
   // 쿨다운 및 마나 비용 검증 관문
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

   // [사격 중 이동 속도 패널티 주입]
   // 사격 모션 동안 플레이어가 느려지도록 기획된 둔화 무기 이펙트를 강제 바인딩합니다.
   if (MovementPenaltyGameplayEffectClass)
   {
       FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
       if (PenaltySpecHandle.IsValid())
       {
           MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
       }
   }

   // 무기 파이어 타입에 따른 물리 판정 엔진 구동
   ExecuteWeaponHitDetection(WeaponData, Character);

   // 만약 공격 애니메이션 몽타주가 등록 안 된 기본 상태라면 예외적으로 0.2초 타이머 버퍼 후 자동 클리어 종료
   if (WeaponData->AttackMontage == nullptr)
   {
       GetWorld()->GetTimerManager().SetTimer(BurstTimerHandle, [this]()
       {
           OnAttackMontageFinished();
       }, 0.2f, false);
       
       return;
   }

   // 사격 애니메이션 비동기 구동 태스크 개통
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

    // 발사 원점 산출 : 시전자 허리 높이인 지상 60cm 라인 기준 정방향 사출
    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60); 
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    // 무기 속성 장부에 기록된 화기 형태에 따라 분기 처리 집도
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

    // 무기 사거리를 곱해 최종 도달 지점 산출
    FVector End = Start + (Forward * InWeaponData->AttackRange);
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter); // 시전자 본인은 히트스캔 라인 검사에서 제외
    QueryParams.bReturnPhysicalMaterial = false;

    // 시각 보정용 채널인 ECC_Visibility 채널로 단발 레이저 스캔 격발
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
       float FinalDamage = InWeaponData->BaseDamage;

       // [헤드샷 전술 배관 알고리즘]
       // 피격 본 이름 장부에 head 또는 neck이 각인되어 있다면 기획서 배율 가산 처리 가동
       if (InWeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             FinalDamage *= InWeaponData->HeadshotMultiplier;
          }
       }

       if (!BaseDamageEffectClass) return;

       // GAS 데미지 계산서 패킷 패킹 조립
       FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
       EffectContext.AddSourceObject(InCharacter);
       FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
       
       if (SpecHandle.IsValid())
       {
          // SetByCaller 변조 우체통을 통해 최종 연산 대미지 수치를 밀봉 배달
          SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);
          FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
          ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
       }
    }
}

void UFT_NormalAttack::OnAttackMontageFinished()
{
    // 애니메이션 몽타주 종료 타이밍에 수명 주기 안전 종결
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
    
    // 타격 시 넘어갈 기초 대미지 효과 장부 1차 구성
    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
    if (DamageSpecHandle.IsValid())
    {
        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, InWeaponData->BaseDamage);
    }

    // [단발 사격 분기]
    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        FTransform SpawnTransform(Forward.Rotation(), Start);
        
        // 지연 생성을 통해 완벽하게 대미지 장부가 복제 인양된 후 최종 사출 가동
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            InWeaponData->ProjectileClass, SpawnTransform, InCharacter, InCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            Projectile->DamageEffectSpecHandle = DamageSpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
        return;
    }

    // [다중 점사 Burst 사격 분기]
    // 람다 캡처 메모리 오염 방지를 위해 스마트 공유 카운터 및 데이터 주소 라인 락인
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    // 무기 에셋에 적힌 버스트 딜레이 주기에 맞춰 루프 타이머 작동 시동
    World->GetTimerManager().SetTimer(BurstTimerHandle, [this, World, ProjectileClass, Start, Forward, InCharacter, DamageSpecHandle, ShotCounter, MaxShots]() mutable
    {
        if (!World || !ProjectileClass || *ShotCounter >= MaxShots)
        {
            if (World)
            {
                World->GetTimerManager().ClearTimer(BurstTimerHandle);
            }
            return;
        }

        FTransform SpawnTransform(Forward.Rotation(), Start);
        
        // 점사 도중 발사되는 탄환들도 낙인 누락이 없도록 매 틱마다 무결성 지연 생성 결합
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

    // [근접 칼질 전방 박스 영역 산출]
    // 공격 사거리를 중심 축으로 삼아 전방 돌격형 충돌 연동 범위를 가공 조립합니다.
    FVector BoxCenter = StartLocation + (ForwardVector * (InWeaponData->AttackRange * 0.5f));
    FVector BoxHalfExtent = FVector(InWeaponData->AttackRange * 0.5f, 80.0f, 60.0f);
    FQuat BoxRotation = InCharacter->GetActorQuat();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter);

    // 월드 다이내믹 채널을 긁어 근접 궤적에 포착된 적 오브젝트 무리를 일괄 인양
    TArray<FOverlapResult> OverlapResults;
    bool bHit = GetWorld()->OverlapMultiByChannel(OverlapResults, BoxCenter, BoxRotation, ECollisionChannel::ECC_WorldDynamic, FCollisionShape::MakeBox(BoxHalfExtent), QueryParams);

#if !UE_BUILD_SHIPPING
    FColor DebugBoxColor = bHit ? FColor::Magenta : FColor::Cyan;
    DrawDebugBox(GetWorld(), BoxCenter, BoxHalfExtent, BoxRotation, DebugBoxColor, false, 1.5f, 0, 2.5f);
#endif

    if (bHit)
    {
       // 포착된 다중 적 액터 군단을 순회하며 광역 대미지 계산서 순차 발송
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

                // 광역 대상을 GAS 표적으로 매핑하기 위해 전용 액터 어레이 구조체 래핑
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
    // 장부 클리어 시점에 오버플로우 방지를 위해 격발 중이던 다중 점사(Burst) 타이머를 완전히 소각합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
    }

    // 평타 사격 행위가 단절되었으므로 시전 시 부착했던 둔화 무기 이펙트를 깨끗하게 수거 환원합니다.
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (MovementPenaltyActiveHandle.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}