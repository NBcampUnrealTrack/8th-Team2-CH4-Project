// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h" 
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_AliceSkill::UFT_AliceSkill()
    : BaseDamage(30.0f) // [기획 데이터 락인] 회중시계 토끼 환영의 기본 관통 피해량 수치 명세
{
    // 인스턴싱 정책: 영웅 캐릭터마다 이 스킬 객체를 독립적으로 생성(Runtime Instance)해서 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선입력 예측(Local Predicted) 발동으로 멀티플레이어 반응성을 극대화합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 우클릭 스킬 식별을 위한 에셋 태그 주입 구역 (UI 및 스킬 트래커 공명용)
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // [에셋 바인딩 가이드] 앨리스 전용 우클릭 보조 공격의 10초 쿨타임을 제어하는 공용 재사용 대기시간 태그 매핑입니다.
    // 에디터 세팅: 이 태그는 이후 제작될 'GE_Cooldown_RightClick'의 Cooldown Tag 슬롯과 1:1 결합되어 쿨다운 머신을 가동합니다.
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [GAS 마스터 관문 제어] 10초 쿨다운 상태를 선제 검사하고 비용(마나/게이지 등)을 서버-클라이언트 동시 승인 및 소모합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        // 쿨타임 중이거나 침묵 등 상태 이상 시 즉시 어빌리티 수명주기를 안전 종료하여 좀비 잔존을 차단합니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // [안전 예외 방어선] 시전자 포인터 유효성을 정밀 검사하여 런타임 Null 크래시를 원천 방쇄합니다.
    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        UWorld* World = GetWorld();

        // [에셋 세팅 핵심 리뷰 포인트] 무기 데이터 에셋(DA_WeaponData_Alice)에 사출할 BP 투사체가 정상 등록되었는지 검증합니다.
        if (World && WeaponData->ProjectileClass)
        {
            // [물리 좌표 연산] 캐릭터 중심부 바닥 위치에서 전방 눈높이(Z축 +60cm) 및 조준 시선 방향으로 스폰 트랜스폼을 동기화합니다.
            FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
            FVector LaunchDirection = Character->GetActorForwardVector();
            FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

            // [멀티플레이 소유권 설정] 스폰될 액터의 소유주(Owner)와 가해자(Instigator)를 앨리스 본체로 락인합니다.
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = Character;
            SpawnParams.Instigator = Character;
            // 지형이나 타 캐릭터와 미세하게 겹쳐도 스폰이 씹히지 않도록 강제 사출 규칙을 적용합니다.
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // [액터 실전 사출] 직선으로 빠르게 돌진하며 적들을 관통하는 회중시계 토끼 환영 오브젝트를 월드에 생성합니다.
            AActor* SpawnedRabbit = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
            
            if (SpawnedRabbit)
            {
                // =========================================================================================
                //  [기획 데이터 및 블루프린트 연동 가이드라인]
                // 
                // 1. 데이터 에셋 설정 (Data Asset):
                //    - 에디터 내 'DA_WeaponData_Alice' 에셋의 [ProjectileClass] 슬롯에 
                //      실제 제작된 블루프린트인 'BP_AliceRabbitProjectile'을 반드시 장착해야 이 구역이 활성화됩니다.
                // 
                // 2. 투사체 블루프린트 내부 로직 구현 (BP_AliceRabbitProjectile):
                //    - 이 투사체는 충돌 시 파괴(Destroy)되지 않는 '관통형' 오브젝트입니다.
                //    - 반드시 콜리전 프로필을 'Overlap' 채널로 설정하여 모든 적을 관통하도록 세팅해야 합니다.
                //    -  [중요] 적중(OnComponentBeginOverlap)된 대상 영웅/미니언이 발견되면 다음 3가지 이펙트를 적 ASC에 찔러야 합니다.
                // 
                // 3. 런타임 이펙트(Gameplay Effect) 연동 세팅:
                //    - ⓐ Damage GE: 'GE_MasterDamage'를 호출하고 'FTTags::FTCombat::Damage' 주소지(SetByCaller)에
                //                    C++ 기획 수치인 [BaseDamage (30.0f)]을 담아 발송합니다. (GEEC_Damage 연산기 최종 공명)
                //    - ⓑ Slow 디버프 GE: 2초간 이동 속도를 감산시키는 지속시간형 이펙트를 피격자에게 주입합니다.
                //    - ⓒ 태엽 인장 표식 GE: 앨리스전투의 핵심인 시한성 태엽 인장 표식 태그를 피격자 몸에 박아넣습니다.
                // =========================================================================================
            }
        }
    }
    else
    {
        // 캐릭터 본체 데이터 유실 예외 상황 발생 시 즉시 리소스를 해제하여 좀비 릭을 원천 방어합니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 환영 사출 시퀀스가 완료되면 어빌리티 인스턴스를 즉시 클린하게 닫아줍니다.
    // 어빌리티가 닫혀야 인풋 허브가 다음 우클릭 연타 명령을 가로막지 않고 유연하게 버퍼를 열어줍니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 최종 정리 수명주기 관문 가동
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}