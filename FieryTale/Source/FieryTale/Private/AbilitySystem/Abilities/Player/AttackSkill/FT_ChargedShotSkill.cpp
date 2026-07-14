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

    // 자원 및 쿨타임 선커밋 타설 (9초 재사용 대기시간 적용선)
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

    // 💡 [지니의 압착: 광역 타격 중심점 계산 배관]
    // 알라딘 시전자의 전방으로 SkillRange(600)만큼 떨어진 좌표를 원형 타격 센터로 지정합니다.
    FVector SpawnerLocation = Character->GetActorLocation();
    FVector FullTargetLocation = SpawnerLocation + (Character->GetActorForwardVector() * SkillRange);
    FullTargetLocation.Z = SpawnerLocation.Z; // 높이 축 평면 통일하여 연산 괴리 방지

    // 💡 [서버 주권 격리 타격 정산]: 데미지 판정과 넉백 이펙트 적용은 서버 권한에서만 1회 엄정 집행합니다.
    if (Character->HasAuthority())
    {
        ExecuteGeniusCrushLogic(FullTargetLocation, Character);
    }

    // 💡 [시각적 연출 파이프라인]: 몽타주 재생은 예측 선로를 타고 클라이언트/서버 동시 격발합니다.
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

    // 원형 범위 오버랩 검문선 타설
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
    // 개발 단계 검증용 디버그 스피어 사출
    DrawDebugSphere(World, TargetCenterLocation, SkillRadius, 16, FColor::Orange, false, 2.0f, 0, 3.0f);
#endif

    if (!bHit) return;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* HitActor = Result.GetActor();
        if (!HitActor) continue;

        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
        if (!TargetASC) continue;

        // 팩션 검문선: 아군 오사 철저 배제
        bool bIsSameTeam = false;
        if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
        else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;

        if (bIsSameTeam) continue;

        // 💡 [핵심: 중심부에서 바깥쪽으로 밀쳐내는 방사형 넉백 벡터 연산]
        FVector TargetActorLoc = HitActor->GetActorLocation();
        FVector KnockbackDirection = TargetActorLoc - TargetCenterLocation; // "중심점 ➡️ 피격자 위치" 벡터 추출
        KnockbackDirection.Z = 0.0f; // 수평 넉백 동기화

        if (KnockbackDirection.IsNearlyZero())
        {
            // 만약 정확히 정중앙에 서있어서 벡터가 0이라면 시전자의 전방 벡터를 백업으로 부여합니다.
            KnockbackDirection = InCharacter->GetActorForwardVector();
        }
        else
        {
            KnockbackDirection.Normalize();
        }

        // 최종 합산 방사형 넉백 화력 세팅
        FVector FinalKnockbackVector = KnockbackDirection * KnockbackForce;

        // GE 스펙 장부에 대미지(50) 및 방사형 넉백 벡터 주입
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
            
            // 💡 넉백 방향 벡터 정보를 SetByCaller를 통해 전달하거나, 필요 시 GE의 Context에 주입합니다.
            // 여기서는 기존 장부 규격인 Magnitude에 힘을 실어주되 피격자 컴포넌트 펄스 연산이나 버프 내에서 활용하도록 전달합니다.
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Knockback, KnockbackForce);
            
            FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(HitActor);
            ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetDataHandle);
            
            // 💡 [강제 넷코드 물리 주입]: 타깃 폰의 무브먼트 퓨즈에 즉각 방사형 충격량(Impulse)을 직접 때려 박아 팅겨냅니다.
            if (ACharacter* TargetChar = Cast<ACharacter>(HitActor))
            {
                if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
                {
                    MoveComp->AddImpulse(FinalKnockbackVector, true);
                }
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