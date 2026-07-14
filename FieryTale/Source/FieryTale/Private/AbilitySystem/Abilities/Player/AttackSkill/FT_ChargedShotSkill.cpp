// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/AttackSkill/FT_ChargedShotSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UFT_ChargedShotSkill::UFT_ChargedShotSkill()
    : BaseDamage(50.0f)      
    , KnockbackForce(800.0f) 
    , SkillRange(600.0f)     // 알라딘 전방으로 지니의 주먹이 떨어질 중심 거리
    , SkillRadius(250.0f)    // 지니의 주먹 종말 타격 원형 반경 (범위)
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

    // 어빌리티 활성화 조건 및 코스트를 검증합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!Character || !GetWorld())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 타격 중심점 계산
    FVector SpawnerLocation = Character->GetActorLocation();
    FVector FullTargetLocation = SpawnerLocation + (Character->GetActorForwardVector() * SkillRange);
    FullTargetLocation.Z = SpawnerLocation.Z;

    // 서버에서 광역 피해 및 넉백 처리 로직을 실행합니다.
    if (Character->HasAuthority())
    {
        ExecuteGeniusCrushLogic(FullTargetLocation, Character);
    }

    // 몽타주 재생 태스크 실행
    bool bTriggeredVisualTask = false;
    if (FireMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("GenieCrushTask"), FireMontage, 1.0f);

        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_ChargedShotSkill::OnFireMontageFinished);
            
            MontageTask->ReadyForActivation();
            bTriggeredVisualTask = true;
        }
    }

    if (!bTriggeredVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_ChargedShotSkill::ExecuteGeniusCrushLogic(const FVector& TargetCenterLocation, AFTPlayerCharacterBase* InCharacter)
{
    UWorld* World = GetWorld();
    UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
    if (!World || !InCharacter || !MyASC || !DamageEffectClass) return;

    // 타격 범위 내 대상 탐색
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(InCharacter);

    TArray<FOverlapResult> OverlapResults;
    bool bHit = World->OverlapMultiByChannel(
        OverlapResults, 
        TargetCenterLocation, 
        FQuat::Identity, 
        ECollisionChannel::ECC_WorldDynamic, 
        FCollisionShape::MakeSphere(SkillRadius), 
        QueryParams
    );

#if !UE_BUILD_SHIPPING
    DrawDebugSphere(World, TargetCenterLocation, SkillRadius, 16, FColor::Orange, false, 2.0f, 0, 3.0f);
#endif

    if (!bHit) return;

    // 대미지 이펙트 마스터 스펙 생성
    FGameplayEffectSpecHandle MasterSpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
    if (!MasterSpecHandle.IsValid() || !MasterSpecHandle.Data.IsValid()) return;

    // 마스터 스펙에 정보 설정
    MasterSpecHandle.Data->GetContext().AddInstigator(InCharacter, InCharacter);
    MasterSpecHandle.Data->GetContext().AddSourceObject(InCharacter);
    MasterSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    MasterSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Knockback, KnockbackForce);

    // 탐색된 대상들에게 처리 진행
    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* HitActor = Result.GetActor();
        if (!IsValid(HitActor)) continue;

        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
        if (!TargetASC) continue;

        // 피아 식별 검증
        bool bIsSameTeam = false;
        if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
        else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;

        if (bIsSameTeam) continue;

        // 생존 여부 검증
        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

        // 넉백 벡터 계산
        FVector TargetActorLoc = HitActor->GetActorLocation();
        FVector KnockbackDirection = TargetActorLoc - TargetCenterLocation; 
        KnockbackDirection.Z = 0.0f; // 수평 넉백 동기화

        if (KnockbackDirection.IsNearlyZero())
        {
            KnockbackDirection = InCharacter->GetActorForwardVector();
        }
        else
        {
            KnockbackDirection.Normalize();
        }

        FVector FinalKnockbackVector = KnockbackDirection * KnockbackForce;

        // 마스터 스펙을 복제하여 개별 컨텍스트 생성
        FGameplayEffectSpec LocalSpec(*MasterSpecHandle.Data.Get());
        FGameplayEffectContextHandle LocalContext = LocalSpec.GetContext().Duplicate();

        FHitResult IndividualHit;
        IndividualHit.HitObjectHandle = HitActor;
        IndividualHit.Location = TargetActorLoc;
        IndividualHit.ImpactPoint = TargetActorLoc;
        
        LocalContext.AddHitResult(IndividualHit, true);
        LocalSpec.SetContext(LocalContext);

        // 대상에게 대미지 이펙트 적용
        MyASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
        
        // 대상에게 넉백 충격량 적용
        if (ACharacter* TargetChar = Cast<ACharacter>(HitActor))
        {
            if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
            {
                MoveComp->AddImpulse(FinalKnockbackVector, true);
            }
        }
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