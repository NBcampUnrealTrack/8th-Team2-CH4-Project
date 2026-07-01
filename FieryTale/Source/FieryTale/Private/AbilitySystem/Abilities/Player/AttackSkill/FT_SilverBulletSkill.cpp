// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"

UFT_SilverBulletSkill::UFT_SilverBulletSkill()
    : BaseDamage(50.0f)           // 기획 데이터 락인: 관통 은탄 적중 시 가동될 고정 피해량 수치 명세
    , ChannellingDuration(1.0f)   // 기획 데이터 락인: 은탄을 총구에 장전하기 위해 유지해야 하는 1초 채널링 시간 명세
{
    // 인스턴싱 정책: 영웅 캐릭터마다 고유의 장전 타이머 핸들을 독립 제어합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선입력 예측 시스템을 가동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    // 에셋 바인딩 가이드: 빨간 망토 전용 우클릭 보조 공격의 재사용 대기시간 태그 매핑입니다.
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_SilverBulletSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // GAS 마스터 규격: 우클릭 스킬 쿨다운 상태를 선제 검사하고 비용을 서버와 클라이언트에서 동시 승인 및 소모합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        // 쿨타임 중이거나 기절 상태라면 즉시 어빌리티 수명주기를 안전 종료하여 자원 누수를 차단합니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 1초 장전 시작 시 채널링 상태 태그를 루즈 태그로 시전자 몸에 주입합니다.
        // 런타임 흐름 요약: 이 태그를 통해 캐릭터 이동 컴포넌트의 사격 감속 애니메이션이나 타 행동 차단 인터럽트가 연동됩니다.
        SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 안전 분기벽: 월드가 정상 작동할 때만 정확하게 1초 장전 타임아웃 타이머를 가동합니다.
    UWorld* World = GetWorld();
    if (World)
    {
        // 1초 동안 방해받지 않고 장전에 성공하면 FireSilverBullet 함수를 격발하여 사출 파이프라인으로 이관합니다.
        World->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    // 1초 장전 타이머가 정상 만료되면 어빌리티 종료 단계로 진입하여 안전하게 투사체 격발을 개시합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 방어선 1단계: 장전 도중 적의 하드 CC기를 맞아 스킬이 중도 캔슬당했다면 타이머를 즉시 청소합니다.
    // 이를 통해 장전이 끊겼는데도 1초 뒤에 은탄이 혼자 사출되는 유령 발사 버그를 완벽하게 차단합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
    }

    // 시전자 몸에 부착해 두었던 채널링 상태 표식 태그를 안전하게 수거하여 조작 제한을 해제합니다.
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // 정상 발사 시퀀스 단독 격발: 스킬이 중간에 하드 CC기로 취소되지 않은 클린 상태일 때만 실제 은탄 투사체를 스폰합니다.
    if (!bWasCancelled && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
        if (Character && Character->GetWeaponData())
        {
            UFT_WeaponData* WeaponData = Character->GetWeaponData();
            UWorld* World = GetWorld();

            // 에셋 세팅 핵심 리뷰 포인트: 무기 데이터 에셋의 ProjectileClass 슬롯에 관통 은탄 블루프린트가 있는지 검증합니다.
            if (World && WeaponData->ProjectileClass)
            {
                // 캐릭터 가슴 높이 좌표 및 전방 Forward 시선 정조준 방향으로 스폰 트랜스폼 동기화
                FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                FVector LaunchDirection = Character->GetActorForwardVector();
                FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = Character;
                SpawnParams.Instigator = Character;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                // 지정 전방 방향으로 모든 적을 관통하는 거대한 은탄 오브젝트 액터를 월드에 사출합니다.
                AActor* SpawnedProjectile = World->SpawnActor<AActor>(WeaponData->ProjectileClass, SpawnTransform, SpawnParams);
                
                if (SpawnedProjectile)
                {
                    // 기획 데이터 및 블루프린트 연동 가이드라인
                    // 
                    // 1. 데이터 에셋 설정:
                    //    에디터 내 빨간 망토 무기 데이터 에셋의 ProjectileClass 슬롯에
                    //    은탄 전용 블루프린트인 BP_RedRidingHoodSilverBullet을 장착해야 사출 인프라가 굴러갑니다.
                    // 
                    // 2. 투사체 블루프린트 내부 로직 구현:
                    //    이 투사체는 충돌 시 파괴되지 않고 모든 미니언과 적 영웅을 관통하는 궤적을 그립니다.
                    //    Overlap 프로필 기반으로 적중 대상들의 무리 배열을 실시간 스캔합니다.
                    // 
                    // 3. 런타임 이펙트 연동 세팅:
                    //    포착된 모든 대상의 ASC에 GE_MasterDamage를 주입하며 FTCombat_Damage 우체통에
                    //    C++ 기획 수치인 BaseDamage 50을 담아 피해 연산기로 배달합니다.
                    //    동시에 적들의 다리를 묶는 둔화 디버프 이펙트를 주입하여 2초간 50퍼센트 슬로우를 구현합니다.
                }
            }
        }
    }

    // 중복 쿨다운 제거: ActivateAbility 상단 관문인 CommitAbility에서 이미 쿨타임 처리가 완료되었습니다.
    // 여기서는 추가 연산 없이 표준 가상 함수 수명주기 반환만 가동하여 능력을 온전히 종료합니다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}