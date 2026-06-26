// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/FT_NormalAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UFT_NormalAttack::UFT_NormalAttack()
{
    // [GAS 설정] 인스턴싱 정책: 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다. (중간에 데이터가 꼬이는 걸 방지)
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 설정] 네트워크 실행 정책: 조작하는 클라이언트가 서버의 허락을 기다리지 않고 '선입력(예측)'하여 즉시 발동합니다. (멀티게임 체감 반응속도 향상)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// [단계 1] 스킬 발동 버튼(LMB 클릭)을 눌렀을 때 엔진이 가장 먼저 호출하는 시작 관문입니다.
void UFT_NormalAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
   Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
   // 스킬을 쓸 수 있는 자원(쿨타임, 마나 등)이 충분한지 검사하고 승인 처리를 받습니다. 실패하면 즉시 종료합니다.
   if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);    
      return;
   }
    
   /* // 1. [데이터 로드] 이 스킬을 격발한 캐릭터 본체와 그 캐릭터가 장착 중인 '무기 데이터 에셋'을 안전하게 긁어옵니다.
   AFT_CharacterBase* Character = Cast<AFT_CharacterBase>(ActorInfo->AvatarActor.Get());
   if (!Character || !Character->GetWeaponData())
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }

   // 기획자가 에디터에서 세팅한 캐릭터별 공격 방식/대미지/몽타주 정보 묶음입니다.
   UFT_WeaponData* WeaponData = Character->GetWeaponData();

   // 2. [비주얼 재생 & 대기] 애니메이션 몽타주를 재생하는 비동기 태스크(Task)를 실행합니다.
   UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitTask(
      this, FName("NormalAttackTask"), WeaponData->AttackMontage, 1.0f);

   if (MontageTask)
   {
      // 애니메이션이 무사히 끝나거나(Completed), 다른 행동에 의해 끊겼을 때(Interrupted) 아래 정의된 'OnAttackMontageFinished' 함수가 자동 호출되도록 예약을 걸어둡니다.
      MontageTask->OnCompleted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      MontageTask->OnInterrupted.AddDynamic(this, &UFT_NormalAttack::OnAttackMontageFinished);
      
      // [실전 런타임 핵심 규칙]
      // 기존처럼 코드에서 대미지 판정을 즉시 때리면 모션이 시작되기도 전에 적이 먼저 맞습니다.
      // 따라서 여기서는 몽타주만 틀어둔 채 가만히 대기(Wait)합니다.
      // 이후 애니메이터가 몽타주 타임라인 안에 심어둔 'AnimNotify(애니메이션 노티파이)'가 격발 순간에 탁 터지면, 
      // 블루프린트나 별도 이벤트를 통해 아래에 있는 'ExecuteWeaponHitDetection' 함수를 원격 격발하게 연결합니다.
      
      MontageTask->ReadyForActivation();
   }
   else
   {
      // 애니메이션 몽타주 파일이 비어있거나 재생에 실패했다면, 캐릭터 조작이 먹통이 되지 않도록 즉시 스킬을 정상 종료(락 해제)합니다.
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
   }
   */
}


// [단계 2] 애니메이션의 격발 노티파이 신호를 받아, 실질적인 총기 사격/투사체/근접 판정을 내리는 연산 허브입니다.
void UFT_NormalAttack::ExecuteWeaponHitDetection(class UFT_WeaponData* WeaponData)
{
    if (!WeaponData) return;

    // 조준점(TPS 카메라 크로스헤어) 기준의 현재 격발 위치(눈높이)와 정면 방향 벡터를 계산합니다.
    FVector StartLocation = CurrentActorInfo->AvatarActor->GetActorLocation() + FVector(0, 0, 60); // 캐릭터 골반 기준 위로 60cm 보정 (눈높이)
    FVector ForwardVector = CurrentActorInfo->AvatarActor->GetActorForwardVector();

    // 코드에 캐릭터 이름(빨간망토, 알라딘 등)을 하드코딩하지 않고, 무기 데이터에 적힌 '공격 타입'에 따라 시스템이 알아서 분기합니다.
    switch (WeaponData->FireType)
    {
    case EWeaponFireType::LineTrace:
       // [빨간 망토 스타일] 히트스캔 사격 판정을 실행합니다.
       PerformLineTraceLogic(WeaponData, StartLocation, ForwardVector);
       break;
    case EWeaponFireType::Projectile:
       // [알라딘 / 앨리스 스타일] 물리적인 실체 투사체를 스폰합니다.
       SpawnProjectileLogic(WeaponData, StartLocation, ForwardVector);
       break;
    case EWeaponFireType::Melee:
       // [가구야 공주 스타일] 근접 칼부림 범위 오버랩 검사를 실행합니다.
       PerformMeleeLogic(WeaponData);
       break;
    }
}


// [세부 로직 A] 즉시 발사(LineTrace / 히트스캔) 방식의 사격 판정 및 대미지 처리 엔진
void UFT_NormalAttack::PerformLineTraceLogic(UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    // 무기 데이터 에셋에 설정된 고유 사거리만큼 정면으로 레이저 빔을 늘립니다.
    FVector End = Start + (Forward * WeaponData->AttackRange);
    FHitResult HitResult;
    
    // 마스터 베이스에 정의된 디버그 플래그(bDrawDebugs)가 에디터에서 켜져 있다면, 화면에 붉은색 레이저 선(궤적)을 그려 시각화합니다.
    EDrawDebugTrace::Type DebugType = bDrawDebugs ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    // 월드의 Visibility 채널(시선 충돌체)을 이용해 직선 상에 닿은 적 액터가 있는지 검사합니다.
    bool bHit = UKismetSystemLibrary::LineTraceSingle(
       GetWorld(), Start, End, UEngineTypes::ConvertToTraceType(ECC_Visibility),
       false, TArray<AActor*>(), DebugType, HitResult, true);

    // 무언가 레이저에 걸렸고 유효한 액터라면
    if (bHit && HitResult.GetActor())
    {
       // 기본 바디샷 대미지를 무기 데이터에서 로드합니다.
       float FinalDamage = WeaponData->BaseDamage;

       // 무기 설정상 헤드샷 판정이 허용된 무기(빨간 망토 장총)라면 맞은 부위의 뼈대(Bone) 이름을 대조합니다.
       if (WeaponData->bAllowHeadshot)
       {
          // 충돌한 스켈레탈 메시의 본 네임에 head 또는 neck 단어가 포함되어 있다면 헤드샷으로 판정합니다.
          if (HitResult.BoneName.ToString().Contains(TEXT("head")) || HitResult.BoneName.ToString().Contains(TEXT("neck")))
          {
             // 무기 고유의 헤드샷 배율(2.0배)을 곱해 대미지를 증폭합니다. (30 -> 60)
             FinalDamage *= WeaponData->HeadshotMultiplier;
          }
       }

       // GAS 마스터 대미지 이펙트(GameplayEffect) 상자를 생성합니다.
       FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BaseDamageEffectClass);
       if (SpecHandle.IsValid())
       {
          // 'Damage'라는 이름의 빈 우체통 칸에 최종 연산된 대미지 숫자(30 또는 60)를 꽂아 넣습니다. (SetByCaller 기법)
          SpecHandle.Data->SetSetByCallerMagnitude(FName("Damage"), FinalDamage);
          
          // 멀티플레이 네트워크 선로 상에서 안전하게 타겟팅 정보(맞은 본, 좌표 등)를 실어 나르기 위해 GAS 전용 핸들 상자로 포장합니다.
          FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);
          
          // 최종 포장된 대미지 명세서와 타겟 정보를 넘겨 적에게 피해를 적용합니다. (반환값은 사용하지 않으므로 void 명시 처리)
          (void)ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
       }
    }
}


// [세부 로직 B] 투사체(Projectile) 사출 방식 처리 (알라딘 3연사 / 앨리스 튕김 카드 대응용)
void UFT_NormalAttack::SpawnProjectileLogic(class UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward)
{
    // 무기 데이터에 적힌 1회 격발당 발수(알라딘 기류 탄환은 3발)만큼 반복 루프를 설정합니다.
    for (int32 i = 0; i < WeaponData->ProjectilesPerShot; ++i)
    {
       // TODO: 실제 런타임 구동 시, 단순 한 프레임 For루프는 3발이 겹쳐서 나가므로 타이머 핸들(TimerHandle)을 사용해 BurstDelay 주기마다 발사하도록 구현 예정
       // TODO: 액터 스폰 후 투사체 내부에 데이터 에셋의 BaseDamage와 MaxBounceCount(도탄 횟수)를 주입하는 파이프라인 연동 구역
    }
}


// [세부 로직 C] 근접 판정(Melee) 방식 처리 (가구야 공주 평타 휩쓸기 및 도발 증강 연동 구역)
void UFT_NormalAttack::PerformMeleeLogic(class UFT_WeaponData* WeaponData)
{
    // TODO: 방패 휘두르기 궤적 범위에 맞춘 콜리전 박스/캡슐 오버랩(Overlap) 적 검색 및 군중제어(도발/넉백) 이펙트 연동 구역
}


// [단계 3] 사격 애니메이션이 무사히 끝나거나 취소되었을 때 스킬을 완벽하게 정리하는 청소 구역입니다.
void UFT_NormalAttack::OnAttackMontageFinished()
{
    // GAS 메모리 내부에서 이 어빌리티의 실행 상태를 해제하고 완전히 종료시킵니다. (이게 호출되어야 다음 평타 입력이 가능해짐)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_NormalAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}