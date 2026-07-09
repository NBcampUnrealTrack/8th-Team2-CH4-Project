// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/FT_AttributeSet.h"

UFT_AliceSkill::UFT_AliceSkill()
    : BaseDamage(30.0f)
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

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 자원 커밋 일원화: 불필요한 중복 검문소를 폐쇄하고 CommitAbility 단일 파이프라인으로 일원화하여 자원을 원자적으로 확정합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 비동기 몽타주 재생 및 분기형 콜백 배관망 가동
    bool bHasVisualTask = false;
    if (AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("AliceSkillTask"), AttackMontage, 1.0f);

        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_AliceSkill::HandleSkillInterrupted);
            
            MontageTask->ReadyForActivation();
            bHasVisualTask = true;
        }
    }
    
    // 시계 토끼 환영 독립 투사체 사출 격발
    FireClockRabbit();
    
    // 런타임 레이스 컨디션 안전 우회선: 비주얼 태스크가 생성되지 않았더라도 즉시 꺼버리지 않고 입력받은 순정 인자를 기반으로 클린 마감합니다.
    if (!bHasVisualTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();

    if (World && ClockRabbitProjectileClass && RabbitImpactEffectClass && Character)
    {
        FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.0f + FVector(0, 0, 40.0f);
        FVector LaunchDirection = Character->GetActorForwardVector();

        // 실전 탄도 동기화 타설: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 포워드 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다.
        if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
        {
            UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
            const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            if (AttributeSet)
            {
                const float CurrentSpread = AttributeSet->GetWeaponSpread();
                if (CurrentSpread > 0.0f)
                {
                    // 평타 및 타 영웅 스킬과 완벽히 호환되는 원형 난수 매핑 방식을 활용해 에임 탄퍼짐 편차를 유도합니다.
                    const float ConeHalfAngleRadius = FMath::DegreesToRadians(CurrentSpread);
                    LaunchDirection = FMath::VRandCone(LaunchDirection, ConeHalfAngleRadius);
                }
            }
        }

        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RabbitImpactEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        // [예측 이중 격발 버그 박멸 완치선]
        // 수동 Apply 사출 구문을 완전히 소각하여 예측 충돌을 배제하고 오리지널 데이터 핸들을 투사체에 독점 이관합니다.
        // 이로 인해 시전 즉시 소멸하는 가상 패킷 오류와 대미지 씹힘 현상이 영구 방어됩니다.
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ClockRabbitProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            // 완벽히 생존 소유권이 보장된 오리지널 스펙 장부를 투사체에 고스란히 이관하여 타깃 적중 시 데미지 증발 현상을 원천 차단합니다.
            Projectile->DamageEffectSpecHandle = SpecHandle;
            Projectile->FinishSpawning(SpawnTransform);
        }
    }
}

void UFT_AliceSkill::OnSkillMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::HandleSkillInterrupted()
{
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
        
        // 네트워크 동기화 쿨타임 환불: CC기나 인터럽트로 취소된 경우, 엔진 공식 순정 팩토리 함수인 MakeQuery_MatchAnyOwningTags 제어선으로 개보수 정렬합니다.
        FGameplayTagContainer TargetCooldownTags;
        TargetCooldownTags.AddTag(CooldownTag);
        
        FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TargetCooldownTags);
        
        MyASC->RemoveActiveEffects(CooldownQuery);
    }

    // 인터럽트에 의한 능동 종료 명시 (bWasCancelled = true)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}