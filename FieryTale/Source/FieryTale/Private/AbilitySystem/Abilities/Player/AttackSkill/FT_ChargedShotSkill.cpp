// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : BaseDamage(50.0f)      // 기획 스펙: 지니의 주먹 적중 고정 피해량 50
    , KnockbackForce(800.0f) // 기획 스펙: 중심부 바깥쪽 넉백 물리 강도 설정
{
    // 인스턴싱 정책 캐릭터마다 이 스킬 객체를 독립적으로 생성해서 관리합니다
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책 선입력 예측 발동
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 우클릭 스킬 태그를 에셋 태그에 주입하여 식별 가능하도록 설정합니다
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    // 우클릭 보조 공격 공용 재사용 대기시간 태그 매핑 (데이터 에셋 설정상 알라딘은 9초 쿨타임 작동)
    CooldownTag = FTTags::FTStates::Cooldown_RightClick;
}

void UFT_ChargedShotSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 차징이 시작된 절대 시간을 기록합니다
    ChargeStartTime = GetWorld()->GetTimeSeconds();
    
    // 추후 구현 포인트 차징 스테이지 진입 시 재생할 클라이언트 전용 차징 나이아가라 이펙트 및 루프 사운드 부착 구역
}

void UFT_ChargedShotSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    
    if (Character && !bWasCancelled)
    {
       // 마우스 버튼을 유지한 총 플로팅 플레잉 타임을 계산합니다
       float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;

       // 1초 이상 마우스를 홀드하여 풀 차징 상태에 도달했는지 검사합니다
       if (ChargeDuration >= 1.0f)
       {
          // 풀 차징 조건 만족 시 스킬 쿨타임(9초)을 정상 작동시킵니다
          (void)CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility);
          
          // 캐릭터의 무기 데이터 에셋으로부터 투사체 정보를 로드하여 격발 인프라를 가동합니다
          if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
          {
              UWorld* World = GetWorld();
              if (World && WeaponData->ProjectileClass)
              {
                  // 시전자의 눈높이 위치와 정면 발사 방향 궤적을 땁니다
                  FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                  FVector LaunchDirection = Character->GetActorForwardVector();

                  FActorSpawnParameters SpawnParams;
                  SpawnParams.Owner = Character;
                  SpawnParams.Instigator = Character;
                  SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                  FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                  // 풀 차징 전용 폭발 투사체(지니의 거대한 주먹) 액터를 월드에 사출합니다
                  AActor* SpawnedInwardPunch = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
                  
                  if (SpawnedInwardPunch)
                  {
                      // [기획 구현 가이드] 사출된 지니의 주먹 내부에 피해량(50)과 중심부 바깥 밀쳐내기(넉백) 매커니즘을 연동합니다
                      // 투사체가 지정 원형 범위 중심부에 도달하거나 적과 충돌 시, 
                      // 범위 내의 모든 적 ASC에 Damage(50) 이펙트를 적용하고 LaunchCharacter 기반 외곽 넉백을 연산하도록 블루프린트와 조립됩니다.
                  }
              }
          }
       }
       else
       {
          // 기획 스펙 차징 시간이 1초 미만인 경우 쿨타임을 돌리지 않고 능력을 조기 정리합니다
          // 필요에 따라 불완전 차징 시의 약화된 투사체를 스폰하는 구역으로 활용 가능합니다
       }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}