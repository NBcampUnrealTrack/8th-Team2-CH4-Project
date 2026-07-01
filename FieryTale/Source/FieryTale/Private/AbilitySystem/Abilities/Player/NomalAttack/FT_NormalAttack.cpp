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
{
    // [인스턴싱 정책] 캐릭터마다 각자 무기 스펙, 연사 속도, 점사 버스트 타이머 상태를 격리 관리하기 위해 액터당 인스턴스화 모드로 제어합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [네트워크 실행 정책] 격발 즉시 레이턴시 없이 사격 피드백과 모션이 반응하도록 클라이언트 선입력 예측 시스템을 전개합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [하드 조작 제어 가드벽] 본체 태그 장부를 선제 대조하여 시전자가 행동 불능 상태일 경우 평타 발동을 원천 통제합니다.
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

    // 시전자 캐스팅 검증 및 인게임 무기 데이터 에셋 존재 여부를 최선행 단계에서 스캔 필터링합니다.
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
    
   // 글로벌 쿨다운 및 침묵 상태 이상 여부를 가동 전에 안전 검증합니다.
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

   // [M-2 속도 누수 릭 완벽 철거 구역]
   // 기존의 원시적인 직접 나눗셈 복원 패턴(OriginalMaxWalkSpeed)을 전량 폐기하고 GAS 순정 이관 수술을 단행했습니다.
   // 공격 중 이속 감산 배율이 적용된 전용 GameplayEffect 에셋을 시전자 본인에게 안전하게 부여합니다.
   // 중복 활성화되거나 강제 캔슬되더라도 GAS 코어 엔진이 스택 연산과 소거를 대리 통제하여 속도가 영구히 뒤틀리는 현상을 원천 방쇄합니다.
   if (MovementPenaltyGameplayEffectClass)
   {
       FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
       if (PenaltySpecHandle.IsValid())
       {
           MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
       }
   }

   // 1. 데이터 에셋 설정(FireType)에 따라 3대 물리 히트 스캔 및 투사체 라인을 즉시 사출합니다.
   ExecuteWeaponHitDetection(WeaponData, Character);

   // 2. [시차 릭 보정선 완착] 아트 리소스가 부족해 애니메이션 몽타주가 None인 프리 테스트 상태일 때,
   // 동일 프레임에 어빌리티가 바로 파괴되면 디버그 드로우 선이 화면에 렌더링되기도 전에 월드 주소가 끊겨 증발합니다.
   // 0.2초의 최소 가시 버퍼 타이머를 열어 렌더링 프레임 시간을 확보한 후 탈출하도록 통제합니다.
   if (WeaponData->AttackMontage == nullptr)
   {
       GetWorld()->GetTimerManager().SetTimer(BurstTimerHandle, [this]()
       {
           OnAttackMontageFinished();
       }, 0.2f, false);
       
       return;
   }

   // 애니메이션 몽타주 재생 및 인터럽트 상태 실시간 모니터링 비동기 태스크 가동
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

    // [좌표 버퍼 안정화] 스켈레탈 메쉬가 없는 투명 인간 상태에서도 발밑 바닥 콜라이더 간섭 없이 레이저가 안전하게 뻗도록 피벗 기준 눈높이 버퍼만 조립합니다.
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

    // [하늘 사격 예외 테스트 회로] 지형 충돌 릭 간섭을 완전히 배제하기 위해 시선의 평면 방위 대신 정수리 머리 위 방향으로 레이저를 강제 쏘아올립니다.
    FVector UpVector = InCharacter->GetActorUpVector();
    FVector End = Start + (UpVector * InWeaponData->AttackRange); 
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter); 
    QueryParams.bReturnPhysicalMaterial = false;

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, QueryParams);

#if !UE_BUILD_SHIPPING
    // 명시적 두께 오버로딩과 잔상 유지 버퍼(5.0f)를 주입하여 식별하기 쉽고 묵직한 레이저 기둥을 보장합니다.
    DrawDebugLine(GetWorld(), Start, bHit ? HitResult.ImpactPoint : End, FColor::Red, false, 2.0f, 0, 5.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 30.f, 12, FColor::Yellow, false, 3.0f, 0, 2.0f);
    }
#endif

    // [대미지 우체통 파이프라인] 피격 대상 포착 즉시 최종 대미지 연산 후 타겟 ASC에 데이터 패킷을 사출합니다.
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
          // C-1 버그 박멸 연동 완료: 네이티브 대미지 태그 장부 상수를 명시 주입하여 GEEC_Damage 수신 파이프라인과 완벽히 호환시킵니다.
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
    // 투사체 에셋이 빌드되지 않은 깡통 상태에서도 사출 방향의 무결성을 정밀 감시하기 위해 전방에 가상 드로우 화살표를 투사합니다.
    FVector ArrowEnd = Start + (Forward * 150.f);
    DrawDebugDirectionalArrow(GetWorld(), Start, ArrowEnd, 30.f, FColor::Green, false, 1.5f, 0, 4.0f);
#endif

    if (!InWeaponData->ProjectileClass) return;

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

    // [H-1 좀비 타이머 영구 누수 완치 구역]
    // 발당 탄환 개수 수치가 높은 다중 점사 무기일 때 딜레이 루프 타이머를 개통합니다.
    TSharedPtr<int32> ShotCounter = MakeShared<int32>(0);
    int32 MaxShots = InWeaponData->ProjectilesPerShot;
    float Delay = InWeaponData->BurstDelay;
    UClass* ProjectileClass = InWeaponData->ProjectileClass;

    // 람다 내부 캡처 인자에 [this]를 명시 바인딩하여 무조건 어빌리티의 소유권 멤버 핸들에 연동되도록 개통했습니다.
    World->GetTimerManager().SetTimer(BurstTimerHandle, [this, World, ProjectileClass, Start, Forward, SpawnParams, ShotCounter, MaxShots]() mutable
    {
        // 점사 스택 횟수를 모두 소모했거나 예외가 발생한 순간, 즉각 ClearTimer 브레이크를 밟아 백그라운드에 좀비 스레드가 무한 누적되는 메모리 릭을 완벽 차단합니다.
        if (!World || !ProjectileClass || *ShotCounter >= MaxShots)
        {
            if (World)
            {
                World->GetTimerManager().ClearTimer(BurstTimerHandle);
            }
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

    // [근접 판정 볼륨 조립] 캐릭터 피벗 정면 리치 반경만큼 전방으로 뻗은 다중 충돌 박스 범위를 실시간 생성합니다.
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
    // 공격 범위를 청록색(Cyan)으로 투사하며, 타격 성공 판정 즉시 보라색(Magenta)으로 밝게 반전시켜 직관적인 감시 피드백을 구현합니다.
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

                // [GC 크래시 완치 스트라이크 존] 구형 코드의 원시 포인터 무단 소유권 가로채기 주입(EXCEPTION_ACCESS_VIOLATION)을 전면 분쇄했습니다.
                // 팩토리로 생성된 타겟 데이터 힙 인스턴스를 핸들이 온전하게 소유권을 양도받아 가비지 컬렉터와 함께 안전 수거하도록 정석 아키텍처로 조립 마감했습니다.
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
    // [인터럽트 방어선] 연사 도중 피격 하드 CC를 맞아 스킬이 강제 캔슬되더라도 잔여 점사 타이머를 흔적 없이 소멸시킵니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
    }

    // =========================================================================
    // [M-2 속도 안전 회귀 그물선 완착]
    // 적의 상태 이상 공격에 의해 어빌리티가 중간에 파쇄당하든(bWasCancelled), 정상 연사가 종료되든 상관없이
    // 내 몸에 달라붙어 무브먼트를 감산 제어하던 패널티 GE 핸들을 안전하게 강제 소거합니다.
    // 이 시점에 GAS 엔진이 처음 영웅의 순정 속도 장부 수치 그대로 무결하게 원상 복귀 처리를 집도합니다.
    // =========================================================================
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (MovementPenaltyActiveHandle.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}