// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AladdinUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h" 
#include "GameplayTags/FTTags.h"

UFT_AladdinUltimateAbility::UFT_AladdinUltimateAbility()
    : AttackRange(1500.0f)
    , AttackWidth(300.0f)
    , BaseDamageValue(50.0f)
    , CurrentWishCount(0)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);
}

bool UFT_AladdinUltimateAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive))
        {
            return true;
        }
    }

    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UFT_AladdinUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!IsValid(OwnerActor) || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 윈도우 타이머 안전 초기화
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
    }

    // 콤보 활성화 상태 확인
    bool bIsFirstWish = !SourceASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);

    if (bIsFirstWish)
    {
        // =========================================================================
        // 💡 [최종 버그 완치선 - 부모 가드 및 커밋 격리 타설]
        // 재귀 루프 중 중복 격발을 차단하기 위해 Super 호출과 CommitAbility 파이프라인을 
        // 오직 1타 최초 진입 시점에만 단 한 번 작동하도록 장부를 완전히 가두어 락인합니다.
        // 이로써 2타, 3타 타격 대미지 격발 시 게이지 유령 리필 버그가 100% 영구 박멸됩니다.
        // =========================================================================
        Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

        if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
        CurrentWishCount = 1;
        SourceASC->AddLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
    }
    else
    {
        // 3초 골든타임 유효 시간 내에 연타 성공 시 스택 카운트 전진
        CurrentWishCount++;
    }

    if (bDrawDebugs)
    {
        FString DebugMsg = FString::Printf(TEXT("Aladdin Ultimate: 지니 %d번째 소원 격발"), CurrentWishCount);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
        }
    }

    // 전방 지니 강타 박스 트레이스 연산 실행
    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    if (CurrentWishCount >= 3)
    {
        // 대망의 3타 최종 마감: 콤보 태그 회수 및 수명 주기 정석 마감 마킹
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
        
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                ComboTimeoutTimerHandle,
                this,
                &UFT_AladdinUltimateAbility::ResetComboState,
                3.0f,
                false
            );
        }
        
        UAbilityTask_WaitInputPress* InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, true);
        if (InputTask)
        {
            InputTask->OnPress.AddDynamic(this, &UFT_AladdinUltimateAbility::OnComboInputPressed);
            InputTask->ReadyForActivation();
        }
        else
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        }
    }
}

void UFT_AladdinUltimateAbility::OnComboInputPressed(float TimeWaited)
{
    ActivateAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr);
}

void UFT_AladdinUltimateAbility::ResetComboState()
{
    if (GetWorld() && ComboTimeoutTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
        ComboTimeoutTimerHandle.Invalidate();
    }

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
    }
    CurrentWishCount = 0;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_AladdinUltimateAbility::ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex)
{
    if (!IsValid(OwnerActor) || !SourceASC || !OwnerActor->GetWorld()) return;

    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    ForwardVector.Z = 0.f; 
    ForwardVector.Normalize();
    
    FVector BoxCenter = StartLocation + (ForwardVector * (AttackRange * 0.5f));
    FVector BoxHalfExtent = FVector(AttackRange * 0.5f, AttackWidth, 250.f);
    FQuat BoxOrientation = OwnerActor->GetActorQuat();

    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanBox = FCollisionShape::MakeBox(BoxHalfExtent);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (bDrawDebugs)
    {
        DrawDebugBox(OwnerActor->GetWorld(), BoxCenter, BoxHalfExtent, BoxOrientation, FColor::Blue, false, 1.f, 0, 3.f);
    }
#endif

    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults, BoxCenter, BoxOrientation, ECollisionChannel::ECC_Pawn, ScanBox, QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            // 💡 [액세스 위반 크래시 완전 완치]: 포인터 생존 유효성을 1차 검문합니다.
            if (!IsValid(TargetActor)) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                float FinalDamage = BaseDamageValue;
                
                if (SmashIndex == 3 && SourceASC->HasMatchingGameplayTag(FTTags::FTAugments::Applied_Aladdin_MercyMirage))
                {
                    FinalDamage *= 2.0f;
                }

                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }
            }
        }
    }
}

void UFT_AladdinUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (GetWorld() && ComboTimeoutTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
        ComboTimeoutTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        // 💡 [자가 수급 전선 초기화 마감]:
        // 어빌리티 최종 마감 시점에 부모가 동적으로 얹어두었던 글로벌 궁극기 분류 태그를 
        // 청정하게 동시 해제 수거하여, 다음 궁극기 수급 전선에 락이 걸리지 않도록 멸균 처리합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }
    CurrentWishCount = 0;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}