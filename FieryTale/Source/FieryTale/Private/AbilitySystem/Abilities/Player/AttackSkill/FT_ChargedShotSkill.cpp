// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/FT_AttributeSet.h"

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : BaseDamage(50.0f)      
    , KnockbackForce(800.0f) 
{
    // 인스턴싱 정책 및 네트워크 실행 정책을 로컬 예측 규격으로 확정합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 어빌리티 고유 자산 태그 등록 및 쿨타임 추적용 태그 매핑을 수행합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);

    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_ChargedShotSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // GAS 예측 아키텍처 규격 확정: 어빌리티 가동 최초 시점에 자원 원자적 선커밋 타설 완료
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!GetWorld())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 인풋 릴리즈 네트워크 복제 활성화 조준 해제 패킷 동기화 선로 개통
    UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    if (ReleaseTask)
    {
        ReleaseTask->OnRelease.AddDynamic(this, &UFT_ChargedShotSkill::FireChargedShot);
        ReleaseTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::FireChargedShot(float TimePressed)
{
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid() || !CurrentActorInfo->AbilitySystemComponent.IsValid() || !GetWorld())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
    bool bTriggeredVisualTask = false;

    if (Character && MyASC)
    {
        // 1초 정밀 충전 성공 판정선
        if (TimePressed >= 1.0f)
        {
            if (UFT_WeaponData* WeaponData = Character->GetWeaponData())
            {
                UWorld* World = GetWorld();
                
                if (World && WeaponData->ProjectileClass && DamageEffectClass)
                {
                    FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
                    FVector LaunchDirection = Character->GetActorForwardVector();

                    // 실전 탄도 동기화 타설: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 포워드 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다
                    const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
                    if (AttributeSet)
                    {
                        const float CurrentSpread = AttributeSet->GetWeaponSpread();
                        if (CurrentSpread > 0.0f)
                        {
                            // 평타 및 앨리스 스킬과 완벽히 호환되는 원형 난수 매핑 방식을 활용해 에임 탄퍼짐 편차 유도
                            const float ConeHalfAngleRadius = FMath::DegreesToRadians(CurrentSpread);
                            LaunchDirection = FMath::VRandCone(LaunchDirection, ConeHalfAngleRadius);
                        }
                    }

                    FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

                    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
                    if (SpecHandle.IsValid())
                    {
                        SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
                        SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Knockback, KnockbackForce);
                    }

                    // 예측 이중 격발 버그 박멸 완치선: 수동 Apply 사출 구문을 완전히 소각하여 예측 충돌을 배제하고 오리지널 데이터 핸들을 투사체에 독점 이관합니다.
                    // 이로 인해 충돌 오버랩 시점에만 대미지와 넉백 판정이 정확히 들어가 가상 좀비 패킷 오류와 대미지 씹힘 현상이 영구 방어됩니다.
                    AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
                        WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                    
                    if (Projectile)
                    {
                        // 완벽히 생존 소유권이 보장된 오리지널 스펙 장부를 투사체에 고스란히 이관하여 타깃 적중 시 데미지와 넉백 증발 현상을 원천 차단합니다
                        Projectile->DamageEffectSpecHandle = SpecHandle;
                        Projectile->FinishSpawning(SpawnTransform);
                    }
                    
                    if (FireMontage)
                    {
                        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                            this, TEXT("AladdinFireTask"), FireMontage, 1.0f);

                        if (MontageTask)
                        {
                            MontageTask->OnCompleted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            MontageTask->OnInterrupted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            MontageTask->OnCancelled.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
                            
                            MontageTask->ReadyForActivation();
                            bTriggeredVisualTask = true;
                        }
                    }
                }
            }
        }
        // 차징 실패 낙폭 구역: 패널티 프리 면제 환원 수선
        else
        {
            // 네트워크 동기화 쿨타임 환불: 엔진 공식 순정 팩토리 함수인 MakeQuery_MatchAnyOwningTags 제어선으로 개보수 정렬합니다
            FGameplayTagContainer TargetCooldownTags;
            TargetCooldownTags.AddTag(CooldownTag);
            
            FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
            MyASC->RemoveActiveEffects(CooldownQuery);
            
            // 재귀적 인터럽트 버그 박멸 완치선: 안전한 기획적 정상 마감 처리
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
            return;
        }
    }
    
    if (!bTriggeredVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::OnFireMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_ChargedShotSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}