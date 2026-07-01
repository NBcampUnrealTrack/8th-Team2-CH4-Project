// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : ChargeStartTime(0.0f)
    , BaseDamage(50.0f)      // 기획 데이터 락인: 풀 차징 시 발동되는 폭발 피해량 기본 수치 명세
    , KnockbackForce(800.0f) // 기획 데이터 락인: 폭발 중심점으로부터 적들을 밀쳐내는 넉백 물리 수치 명세
{
    // 인스턴싱 정책: 영웅 캐릭터마다 이 차징 스킬 객체를 독립적으로 생성하여 고유의 차징 시간을 기록합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선입력 예측 발동으로 조준 및 차징 시작 타이밍의 반응성을 극대화합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 알라딘 전용 우클릭 보조 공격의 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 우클릭 공용 쿨타임 이펙트 에셋의 Cooldown Tag 슬롯과 결합됩니다.
    // 앨리스와 달리 알라딘은 풀 차징 스케줄이 완료되는 순간 소스코드 하단에서 수동으로 격발시킵니다.
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_ChargedShotSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 무한 차징 핵 차단: 쿨타임 중이거나 기절, 침묵 등 상태 이상 락이 걸렸다면 차징 시작 자체를 거부합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 마우스 우클릭 버튼을 누른 절대 시간을 안전하게 기록하여 홀드 타임 측정 기저를 마련합니다.
        ChargeStartTime = World->GetTimeSeconds();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::FireChargedShot()
{
    // GAS 정석 배관 수술: 외부 인풋 릴리즈 신호를 받아 가동되는 핵심 사출 함수입니다.
    // 타겟 액터나 월드가 유효하지 않다면 즉시 수명주기를 강제 캔슬 상태로 닫아 릭을 차단합니다.
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid() || !GetWorld())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
    if (Character)
    {
        // 마우스 버튼을 누르고 유지한 총 플로팅 플레이 타임을 실시간 계산합니다.
        float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;

        // 기획서 기준: 1초 이상 마우스를 홀드하여 풀 차징 상태에 도달했는지 검사합니다.
        if (ChargeDuration >= 1.0f)
        {
            // 풀 차징 조건 만족 시에만 우클릭 스킬 쿨타임을 공식적으로 마킹 및 격발시킵니다.
            CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
            
            // 캐릭터의 무기 데이터 에셋으로부터 풀 차징 전용 사출용 투사체 정보를 로드합니다.
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                UWorld* World = GetWorld();
                if (World && WeaponData->ProjectileClass)
                {
                    // 캐릭터 중심부 바닥 위치에서 전방 눈눈높이 물리 좌표 및 시선 방향 트랜스폼 동기화
                    FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                    FVector LaunchDirection = Character->GetActorForwardVector();

                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = Character;
                    SpawnParams.Instigator = Character;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                    // 풀 차징 전용 광역 폭발 투사체인 지니의 거대한 주먹 액터를 월드에 사출합니다.
                    AActor* SpawnedInwardPunch = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
                    
                    if (SpawnedInwardPunch)
                    {
                        // 기획 데이터 및 블루프린트 연동 가이드라인
                        // 
                        // 1. 데이터 에셋 설정:
                        //    에디터 내 알라딘 무기 데이터 에셋의 ProjectileClass 슬롯에
                        //    풀 차징 전용 폭발 블루프린트인 BP_AladdinChargedPunch를 장착해야 작동합니다.
                        // 
                        // 2. 투사체 블루프린트 내부 로직 구현:
                        //    이 투사체는 적 벽이나 미니언에 충돌하거나 최대 사거리에 도달 시 폭발하며 소멸합니다.
                        //    폭발 시 범위 컴포넌트나 피어싱 오버랩 배열을 확보하여 반경 내 모든 적을 탐색합니다.
                        // 
                        // 3. 런타임 이펙트 연동 세팅:
                        //    폭발 범위에 걸린 모든 피격 대상들의 ASC에 다음 2가지 연산을 전동합니다.
                        //    첫째, GE_MasterDamage를 주입하며 FTCombat_Damage 주소지에 BaseDamage 수치인 50을 담아 발송합니다.
                        //    둘째, 캐릭터 무브먼트 컴포넌트를 견인하거나 물리 충격을 가해 KnockbackForce 수치인 800만큼 외곽으로 넉백시킵니다.
                    }
                }
            }
        }
        else
        {
            // 1초 미만 불완전 차징 시에는 쿨타임을 가동하지 않고 취소 처리하는 기획 스펙 완착 구역입니다.
            // 쿨다운 리스크 없이 즉시 마우스 우클릭을 재입력하여 다시 차징을 시도할 수 있는 상태를 유지합니다.
        }
    }

    // 사출 연산이 종료된 직후 표준 수명주기 반환 관문을 가동하여 능력을 정상 종료합니다.
    // 능력이 정상적으로 닫혀야 다음 입력 버퍼 가두리가 열려 평타나 다른 스킬로 연계가 부드럽게 흘러갑니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_ChargedShotSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 완전 방어: 차징 도중 적의 하드 CC기를 맞아 스킬이 강제 캔슬당했다면 사출 구역을 스킵하고 안전하게 상태 청소만 진행합니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}