// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_SilverBulletSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UFT_SilverBulletSkill::UFT_SilverBulletSkill()
    : BaseDamage(50.0f)           // 기획 명세: 관통 은탄 저격 기술의 강력한 고유 깡데미지
    , ChannellingDuration(1.0f)   // 기획 명세: 탄환을 정조준 장전하는 강제 선딜레이 조작 잠금 시간 (1초)
{
    // 인스턴싱 정책: 차징 상태 관리 및 독립적 타이머 가동을 위해 액터당 인스턴스 격리 생성
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선예측 구동 후 서버 검증으로 저격 격발 조작 반응성 확보
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    // GAS 에셋 태그 등록: 스킬 분류 장부에 보조 공격 기술 태그 바인딩
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    // 쿨다운 태그 지정: 마우스 우클릭 스킬 공용 재사용 대기시간 파이프라인(9초)과 자석 연동
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_SilverBulletSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [GAS 순정 시전 비용 검증]
    // 마나와 자원이 부족하다면 즉시 능력을 종료하여 불필요한 조작 잠금 버그를 예방합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = ActorInfo ? Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (SourceASC)
    {
        // 채널링 상태 표식 주입 : 1초 저격 조준 동안 이동을 제한하거나 다른 평타 사격 개입을 완벽히 차단
        SourceASC->AddLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }
    
    if (Character && Character->GetWeaponData())
    {
        UFT_WeaponData* WeaponData = Character->GetWeaponData();
        if (WeaponData && WeaponData->AttackMontage)
        {
            // 저격 장전 자세 애니메이션 비동기 태스크 구동
            UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this, TEXT("SilverBulletChannellingTask"), WeaponData->AttackMontage, 1.0f);

            if (MontageTask)
            {
                // 채널링 도중 적의 하드 CC기를 맞고 몽타주가 파쇄당하면 즉시 파이프라인을 토스하여 사출 없이 파쇄 취소
                MontageTask->OnInterrupted.AddDynamic(this, &UFT_SilverBulletSkill::FireSilverBullet);
                MontageTask->OnCancelled.AddDynamic(this, &UFT_SilverBulletSkill::FireSilverBullet);
                MontageTask->ReadyForActivation();
            }
        }
    }

    UWorld* World = GetWorld();
    if (World)
    {
        // 1초 동안 아무런 CC 방해 없이 정조준 장전에 완벽히 성공하면 격발 시퀀스(FireSilverBullet) 호출
        World->GetTimerManager().SetTimer(ChannellingTimerHandle, this, &UFT_SilverBulletSkill::FireSilverBullet, ChannellingDuration, false);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_SilverBulletSkill::FireSilverBullet()
{
    // 정식 장전 만료 타이머가 끝났거나 하드 CC기로 가드가 터졌을 때, 수명 주기의 핵심 관문인 EndAbility로 이관하여 사출 여부를 종합 제어합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_SilverBulletSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 능력이 종결되는 시점에 예외적인 메모리 릭 방지를 위해 가동 중이던 1초 조준 만료 타이머를 청정하게 제거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChannellingTimerHandle);
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        // 채널링 상태 표식을 소거하여 빨간 망토의 모든 사격 및 조작 락을 정상 복구
        SourceASC->RemoveLooseGameplayTag(FTTags::FTCombat::Skill_Channelling);
    }

    // CC기에 의해 강제 취소당하지 않고(bWasCancelled == false) 1초 집중 저격 장전선을 깔끔하게 성공 완수한 상태인지 판정
    if (!bWasCancelled && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
        if (Character && Character->GetWeaponData())
        {
            UFT_WeaponData* WeaponData = Character->GetWeaponData();
            UWorld* World = GetWorld();
            
            if (World && WeaponData->ProjectileClass && SilverBulletImpactEffectClass)
            {
                // 사출 좌표 연산 : 캐릭터 허리 라인 높이 60cm 기준 정방향 락온 트랜스폼 산출
                FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                FVector LaunchDirection = Character->GetActorForwardVector();
                FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = Character;
                SpawnParams.Instigator = Character;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                // [순정 GAS 관통 은탄 화력 계산서 발행]
                FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SilverBulletImpactEffectClass, GetAbilityLevel());
                if (SpecHandle.IsValid())
                {
                    // SetByCaller 변조 우체통망을 경유하여 기획 고유 고화력 데미지 수치(50.0f)를 명시적 밀봉 주입
                    SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                }

                // 지연 생성(Deferred Spawn) 체계 가동 : 투사체가 월드 배치 전 계산서 장부를 안전하게 전수받을 대기 상태 유도
                AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                    WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                
                if (Projectile)
                {
                    // 완공된 은탄 피해 명세서 패킷을 투사체 내부 배관에 안전하게 토스 전송
                    Projectile->DamageEffectSpecHandle = SpecHandle;
                    
                    // 데이터 결합이 완료되었으므로 전방 전선으로 무결하게 사출 마감
                    Projectile->FinishSpawning(SpawnTransform);
                }
            }
        }
    }

    // 상위 부모의 공정을 격발하여 쿨다운 자원 확정 차감 및 스킬 수명 주기 영구 마감
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}