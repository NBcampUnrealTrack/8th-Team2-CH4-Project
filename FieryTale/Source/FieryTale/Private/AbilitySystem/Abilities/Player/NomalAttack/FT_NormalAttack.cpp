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
    : OriginalMaxWalkSpeed(0.0f) // 이동 속도 복구 연산 왜곡을 차단하기 위한 기본 수치 보관소 초기화
{
    // 인스턴싱 정책: 영웅들마다 독립적인 평타 콤보 및 점사 타이머를 추적하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 공격 버튼 입력 즉시 로컬에서 사격 피드백을 느끼도록 클라이언트 선입력 예측 시스템을 기동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UFT_NormalAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
   // GAS 마스터 관문 제어: 글로벌 공통 평타 내부 재사용 대기시간 및 침묵 상태 이상 여부를 선제 검증합니다.
   if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);    
      return;
   }

   // 안전 방어선: 공격 시점의 시전자 포인터 및 유효한 무기 데이터 에셋 존재 여부를 검사합니다.
   AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
   if (!Character || !Character->GetWeaponData())
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }

   UFT_WeaponData* WeaponData = Character->GetWeaponData();

   // 이속 버그 방어선 1단계: 사격 중 이동 속도 감산 페널티를 연산하기 직전 영웅의 순정 최대 이동 속도를 안전하게 백업합니다.
   // 백업 직후 무기 에셋에 지정된 고유 감속 배율을 곱해 조준 사격 중인 묵직한 캐릭터 상태를 물리 구현합니다.
   if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
   {
      OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
      MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * WeaponData->GetMovementPenaltyMultiplier();
   }

   // 분기 인프라 가동: 데이터 에셋 설정에 정의된 공격 타입에 따라 적절한 물리 스캔을 격발합니다.
   ExecuteWeaponHitDetection(WeaponData, Character);

   // 평타 사격 애니메이션 몽타주 비동기 태스크 구동
   UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

   if (MontageTask)
   {
      // 사격 모션이 정상 완료되거나 다른 스킬 입력으로 캔슬 시 어빌리티 해제 파이프라인으로 넘깁니다.
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

    // 캐릭터 골반 및 바닥 위치에서 전방 시선 눈높이 좌표 버퍼를 생성합니다.
    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60); 
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    // 에셋 세팅 가이드: 에디터 내 해당 무기 데이터 에셋의 FireType 분기 설정에 따라 3대 공격 판정을 자동 스위칭합니다.
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

    // 무기 에셋에 설정된 사거리를 곱해 최종 직선 레이저 궤적 좌표를 연산합니다.
    FVector End = Start + (Forward * InWeaponData->AttackRange); 
    FHitResult HitResult;
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter); 
    QueryParams.bReturnPhysicalMaterial = false;

    // 시선 방향 가시성 채널을 통해 단발성 즉발 히트 스캔을 수행합니다.
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

       // 헤드샷 연산 처리 구역: 기획 스펙 상 헤드샷 기능이 승인된 경우 적 캐릭터의 head 및 neck 본 본네임 판정을 필터링합니다.
       if (InWeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             // 조건 만족 시 에셋에 기입해 둔 헤더 가산 배율을 곱해 치명상을 구현합니다.
             FinalDamage *= InWeaponData->HeadshotMultiplier;
          }
       }

       // 데이터 우체통 사출: 마스터 피해 이펙트 클래스로 수치를 주입합니다.
       FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass, GetAbilityLevel());
       if (SpecHandle.IsValid())
       {
          // 무기 수치 데이터를 FTCombat_Damage 태그 주소지에 실어 피해 연산기(GEEC_Damage)로 안전 배달합니다.
          SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);
          FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
          
          ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
       }
    }
}

void UFT_NormalAttack::OnAttackMontageFinished()
{
    // 공격 모션 시퀀스가 완결되면 어빌리티 종료 표준 관문으로 넘겨 리소스를 통합 정리합니다.
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

    // 단발성 투사체 사출 분기: 발당 탄환 수가 1발 이하인 경우 즉시 사출합니다.
    if (InWeaponData->ProjectilesPerShot <= 1)
    {
        FTransform SpawnTransform(Forward.Rotation(), Start);
        World->SpawnActor<AActor>(InWeaponData->ProjectileClass, SpawnTransform, SpawnParams);
        return;
    }

    // 연사 점사 투사체 사출 분기: 발당 탄환 개수 수치가 높게 세팅된 경우 버스트 딜레이 타이머를 개통합니다.
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    // 버그 방어선 완착: 람다식 내부의 로컬 타이머 릭으로 인한 메모리 오염 및 점사 버그를 차단하기 위해
    // 클래스 멤버 타이머 핸들 변수인 BurstTimerHandle 배관에 타이머를 바인딩하여 안전 통제합니다.
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

    // 근접 무기 박스 판정 연산 구역: 캐릭터 중심 좌표 기준으로 정면 리치 반경만큼 확장된 충돌 박스를 조립합니다.
    FVector StartLocation = InCharacter->GetActorLocation() + FVector(0, 0, 60);
    FVector ForwardVector = InCharacter->GetActorForwardVector();

    FVector BoxCenter = StartLocation + (ForwardVector * (InWeaponData->AttackRange * 0.5f));
    // 에디터 세팅 가이드: 무기 데이터 에셋의 AttackRange 수치 절반이 박스의 앞뒤 사거리가 되며, 좌우 너비 80, 높이 60 버퍼로 강타 범위를 고정합니다.
    FVector BoxHalfExtent = FVector(InWeaponData->AttackRange * 0.5f, 80.0f, 60.0f);
    FQuat BoxRotation = InCharacter->GetActorQuat();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter);

    TArray<FOverlapResult> OverlapResults;
    
    // 근접 판정 전용 다중 월드 오버랩 스캔을 격발합니다.
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
       // 박스 반경 안에 포착된 다중 피격 대상을 순회하며 대미지 사출 파이프라인을 가동합니다.
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

                // 크래시 박멸 완료: 원시 C++ 포인터의 수동 new 할당으로 인한 가비지 메모리 폭사를 완벽하게 철거하고,
                // GAS 공식 구조 공유 규격 핸들러인 MakeShared 팩토리를 조립하여 자동 안전 수거 인프라를 완착합니다.
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
    // 인터럽트 방어선 2단계: 연사 점사 도중 하드 CC나 인터럽트를 맞아 캔슬되더라도 백그라운드 점사 타이머 잔재를 완전히 소멸시켜 탄환 누수 릭을 완벽 디펜스합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
    }

    // 이속 원상복구 확정: 사격 모션이 끝나 탈출하는 시점에, 처음 백업해 두었던 고정 순정 원본 수치 그대로 이동 속도를 안전 회귀 복구시킵니다.
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

    // 최종 내부 리소스 반환 및 종료 가동
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}