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
    DrawDebugSphere(World, TargetCenterLocation, SkillRadius, 16, FColor::Orange, false, 2.0f, 0, 3.0f);
#endif

    if (!bHit) return;

    // =========================================================================
    // 💡 [4단계 명세: 메모리 낭비 제거 초적화 마스터 레이어 개통]
    // 루프 바깥에서 공용 대미지 계산서(Spec)를 딱 1장만 선제 발행하여 힙 할당 빈도를 1회로 고정합니다.
    // =========================================================================
    FGameplayEffectSpecHandle MasterSpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
    if (!MasterSpecHandle.IsValid() || !MasterSpecHandle.Data.IsValid()) return;

    // 시전자 정보 및 베이스 대미지/넉백 수치를 마스터 장부에 단 1회 락인 밀봉합니다.
    MasterSpecHandle.Data->GetContext().AddInstigator(InCharacter, InCharacter);
    MasterSpecHandle.Data->GetContext().AddSourceObject(InCharacter);
    MasterSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
    MasterSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Knockback, KnockbackForce);

    // 준비된 마스터 계산서 1장을 들고 오버랩된 적들 고속 루프 정산 전개
    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* HitActor = Result.GetActor();
        if (!IsValid(HitActor)) continue;

        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
        if (!TargetASC) continue;

        // 팩션 검문선: 아군 오사 철저 배제
        bool bIsSameTeam = false;
        if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsSameTeam = true;
        else if (MyASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsSameTeam = true;

        if (bIsSameTeam) continue;

        // 타깃 시체 구타 방지 검문
        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

        // 💡 중심부에서 바깥쪽으로 밀쳐내는 방사형 넉백 벡터 연산
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

        // =========================================================================
        // 💡 [고성능 장부 돌려막기]: 신규 힙 할당 없이 복사 생성자로 가벼운 스택 사본 장부만 복제!
        // 개별 타깃의 히트 결과 정보만 사본의 컨텍스트에 스냅샷 저장하여 직통 사출합니다.
        // =========================================================================
        FGameplayEffectSpec LocalSpec(*MasterSpecHandle.Data.Get());
        FGameplayEffectContextHandle LocalContext = LocalSpec.GetContext().Duplicate();

        FHitResult IndividualHit;
        IndividualHit.HitObjectHandle = HitActor;
        IndividualHit.Location = TargetActorLoc;
        IndividualHit.ImpactPoint = TargetActorLoc;
        
        LocalContext.AddHitResult(IndividualHit, true);
        LocalSpec.SetContext(LocalContext);

        // 공격자의 ASC가 타깃의 ASC에 직접 대미지 및 스탯 장부 집행!
        MyASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
        
        // 타깃 폰의 무브먼트에 방사형 충격량(Impulse) 직접 때려 박기 (넉백)
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