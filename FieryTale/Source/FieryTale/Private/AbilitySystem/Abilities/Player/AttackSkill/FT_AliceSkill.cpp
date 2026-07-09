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
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

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

    // 순정 GAS 가드선 복구: 중복 검문을 소각하고 CommitAbility 단일 파이프라인으로 일원화하여 쿨타임과 코스트 장부를 원자적으로 검문 및 차단인합니다
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
    
    // 레이스 컨디션 안전 우회선: 비주얼 태스크가 생성되지 않았더라도 즉시 꺼버리지 않고 입력받은 순정 인자가 기반으로 클린 마감합니다
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

        // 실전 탄도 동기화 타설: 시전자의 WeaponSpread 속성값을 실시간 인양하여 전방 포워드 벡터에 무작위 회전 반경을 정밀 계산하여 주입합니다
        if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
        {
            UAbilitySystemComponent* MyASC = CurrentActorInfo->AbilitySystemComponent.Get();
            const UFT_AttributeSet* AttributeSet = Cast<UFT_AttributeSet>(MyASC->GetAttributeSet(UFT_AttributeSet::StaticClass()));
            if (AttributeSet)
            {
                const float CurrentSpread = AttributeSet->GetWeaponSpread();
                if (CurrentSpread > 0.0f)
                {
                    // 평타와 동일한 규격의 원형 난수 매핑 방식을 활용해 에임 탄퍼짐 편차 유도
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

        // 코어 예측 배관 교정 가드선 완공: Apply 연산 시 핸들이 소멸소각되는 현상을 가드하기 위해 복사본 사본장부를 선제 타설합니다
        FGameplayEffectSpecHandle PredictCommitHandle = SpecHandle;

        // GAS 예측 수명선 마감: 사본 장부를 제출함으로써 예측 좀비 릭을 완벽히 소각 마감합니다
        FGameplayAbilityTargetDataHandle EmptyTargetDataHandle;
        ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, PredictCommitHandle, EmptyTargetDataHandle);

        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            ClockRabbitProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            // 완벽히 생존 소유권이 보장된 오리지널 스펙 장부를 투사체에 고스란히 이관하여 타깃 적중 시 데미지 증발 현상을 원천 차단합니다
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
        
        // 순정 엔진 팩토리 파이프라인 정순 배치: 가공의 명칭을 영구 축출하고 이펙트가 소유한 쿨타임 태그 자체를 정밀 타깃팅하도록 MakeQuery_MatchAnyEffectTags 정적 함수를 조립해 무기 스왑 및 상태 이상 캔슬 시 환불 처리를 무결하게 마감합니다
        FGameplayTagContainer TargetCooldownTags;
        TargetCooldownTags.AddTag(CooldownTag);
        
        FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(TargetCooldownTags);
        
        MyASC->RemoveActiveEffects(CooldownQuery);
    }

    // 인터럽트에 의한 능동 종료 명시 bWasCancelled 는 true 로 반환합니다
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}