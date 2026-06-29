// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    // [GAS 설정] 인스턴싱 정책: 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 설정] 네트워크 실행 정책: 선입력(예측) 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// [단계 1] 스킬 발동 버튼(LMB 클릭) 관문
void UFT_NormalAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
   // 1. [순정 복구] 스킬을 쓸 수 있는 자원(쿨타임, 마나 등)이 충분한지 검사하고 승인 처리
   if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);    
      return;
   }

   // 2. [데이터 로드] 캐릭터 본체와 무기 데이터 에셋 안전하게 로드
   AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
   if (!Character || !Character->GetWeaponData())
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }

   UFT_WeaponData* WeaponData = Character->GetWeaponData();

   // 3. [순정 복구] 애니메이션 몽타주를 재생하는 비동기 태스크(Task) 가동
   UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

   if (MontageTask)
   {
      // 애니메이션이 끝나거나 끊겼을 때 어빌리티를 정리하도록 예약
      MontageTask->OnCompleted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      MontageTask->OnInterrupted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      
      // 애프터 이펙트나 블루프린트 AnimNotify 등에서 ExecuteWeaponHitDetection를 원격으로 때릴 수 있도록 준비
      MontageTask->ReadyForActivation();
      
      //  [선택 사양] 만약 아직 노티파이 바인딩이 안 되어 있고, 몽타주를 틀면서 "즉시" 판정을 보고 싶다면
      // 아래 주석을 풀고 몽타주 재생과 동시에 판정을 실행하게 하셔도 됩니다.
      // ExecuteWeaponHitDetection(WeaponData, Character);
   }
   else
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }
}


// [단계 2] 실질적인 사격/투사체/근접 판정을 내리는 연산 허브
void UFT_NormalAttack::ExecuteWeaponHitDetection(class UFT_WeaponData* WeaponData, class AFTPlayerCharacterBase* Character)
{
    if (!WeaponData || !Character) return;

    // 안전한 Character 포인터로부터 눈높이 위치와 정면 벡터를 구합니다.
    FVector StartLocation = Character->GetActorLocation() + FVector(0, 0, 60); 
    FVector ForwardVector = Character->GetActorForwardVector();

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


// [세부 로직 A] 즉시 발사(LineTrace) 방식 처리
void UFT_NormalAttack::PerformLineTraceLogic(UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    // 4. [순정 복구] 무기 데이터 에셋에 설정된 고유 사거리(AttackRange)를 다시 연동합니다!
    FVector End = Start + (Forward * WeaponData->AttackRange); 
    FHitResult HitResult;
    
    bool bHit = UKismetSystemLibrary::LineTraceSingle(
       GetWorld(), 
       Start, 
       End, 
       ETraceTypeQuery::TraceTypeQuery1,
       false, 
       TArray<AActor*>(), 
       EDrawDebugTrace::ForDuration, // 디버그 선 시각화 유지
       HitResult, 
       true
    );

    if (bHit && HitResult.GetActor())
    {
       float FinalDamage = WeaponData->BaseDamage;

       if (WeaponData->bAllowHeadshot)
       {
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             FinalDamage *= WeaponData->HeadshotMultiplier;
          }
       }

       FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass);
       if (SpecHandle.IsValid())
       {
          SpecHandle.Data->SetSetByCallerMagnitude(FName("Damage"), FinalDamage);
          FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
          (void)ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
       }
    }
}


void UFT_NormalAttack::SpawnProjectileLogic(class UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    for (int32 i = 0; i < WeaponData->ProjectilesPerShot; ++i)
    {
       // 대기 중 구역
    }
}


void UFT_NormalAttack::PerformMeleeLogic(class UFT_WeaponData* WeaponData)
{
    // 대기 중 구역
}


// [단계 3] 애니메이션이 끝나거나 취소되었을 때 정상적으로 락을 푸는 구역
void UFT_NormalAttack::OnAttackMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}