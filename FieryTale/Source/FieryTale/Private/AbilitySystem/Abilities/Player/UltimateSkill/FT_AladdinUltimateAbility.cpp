// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AladdinUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
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

void UFT_AladdinUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
    }

    // 콤보 윈도우 판정: 시전자 몸에 콤보 활성화 버프 태그가 없다면 최초 시전인 최초 타격 상태로 판정합니다.
    bool bIsFirstWish = !SourceASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);

    if (bIsFirstWish)
    {
        // 최초 시전 시점에만 궁극기 게이지 자원 비용 소모를 공식 커밋합니다.
        if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
        CurrentWishCount = 1;
    }
    else
    {
        // 유효 시간 내에 재진입했다면 게이지를 추가 소모하지 않고 콤보 스택 단계만 전진시킵니다.
        CurrentWishCount++;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (bDrawDebugs)
    {
        FString DebugMsg = FString::Printf(TEXT("Aladdin Ultimate: 지니 %d번째 소원 격발"), CurrentWishCount);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
        }
    }

    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    // 콤보 윈도우 유효 시간 및 정리 파이프라인 제어 구역
    if (CurrentWishCount >= 3)
    {
        // 마지막 타격 완료 분기: 최종 단계까지 모두 완수했다면 시전자 몸에서 콤보 대기 태그를 회수하고 스택을 원상태로 청소합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
        
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 연사 대기 분기: 후속 타격 직전에는 시전자 몸에 다음 콤보 가능 표식 태그를 부착합니다.
        SourceASC->AddLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        
        // 3초 내에 다음 입력을 하지 않아 콤보가 끊기면 자동으로 좀비 스택과 태그를 수거하는 안전 타이머를 가동합니다.
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
        
        // 능력을 즉시 반환 종료시켜 주어야 키 입력 컴포넌트가 다음 연타 신호를 차단하지 않고 재진입 통로를 열어줍니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AladdinUltimateAbility::ResetComboState()
{
    // 안전 수거 함수: 입력 타임아웃 도달 시 백그라운드에서 유령 잔재 태그와 스택 카운트를 소멸시킵니다.
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
}

void UFT_AladdinUltimateAbility::ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex)
{
    if (!OwnerActor || !SourceASC || !OwnerActor->GetWorld()) return;

    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    ForwardVector.Z = 0.f; // 지형 고저차 경사로로 인한 전방 박스 궤적의 회전 왜곡을 원천 차단합니다.
    ForwardVector.Normalize();
    
    FVector BoxCenter = StartLocation + (ForwardVector * (AttackRange * 0.5f));
    
    // 고저차 보정 완착: 한타 교전 도중 계단이나 미세 언덕 지형에서 판정이 증발하는 현상을 막기 위해 범위를 확장합니다.
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
        OverlapResults,
        BoxCenter,
        BoxOrientation,
        ECollisionChannel::ECC_Pawn,
        ScanBox,
        QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                float FinalDamage = BaseDamageValue;
                
                // 증강 카드 링크 연동: 최종 단계 격발 시점에 알라딘 전용 자비의 신기루 증강 카드를 보유 중이라면 대미지 배율을 주입합니다.
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
    // 인터럽트 방어선 완료: 콤보 시전 도중에 적의 하드 CC기를 맞아 비정상 취소되면 타이머를 끄고 스택 카운트를 초기화합니다.
    if (GetWorld() && ComboTimeoutTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
        ComboTimeoutTimerHandle.Invalidate();
    }

    if (bWasCancelled)
    {
        UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
        if (SourceASC)
        {
            SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        }
        CurrentWishCount = 0;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}