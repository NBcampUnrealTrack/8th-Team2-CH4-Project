// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AladdinUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h" // ◄ 콤보 연타 실시간 감시 태스크 인클루드 완착
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

// =========================================================================
// 💡 [궁극기 자원 차단선 개통]: 게이지가 100% 미만일 때 궁극기가 강제 격발되는 
// 자원 누수 버그를 완치하기 위해 GAS 정석 선행 검문소를 타설합니다.
// =========================================================================
bool UFT_AladdinUltimateAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // 이미 콤보가 가동 중인 연타 상태(2타, 3타 지점)라면 비용 검문을 무결하게 프리패스 시켜줍니다.
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
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    
    if (!OwnerActor || !SourceASC)
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
        // 1타 격발 최초 시점에만 궁극기 전량 게이지 자원 소모 장부를 커밋 처리합니다.
        if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
        CurrentWishCount = 1;
        // 콤보 가동 중 태그 장부 점등
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

    // =========================================================================
    // 💡 [3단 콤보 수명주기 무결성 처리]: 1~2타 지점에서는 EndAbility를 부르지 않고,
    // 인풋 프레스 감시 태스크를 가동해 수명선을 살려둔 채로 다음 연타 입력을 비동기 대기합니다.
    // =========================================================================
    if (CurrentWishCount >= 3)
    {
        // 대망의 3타 최종 마감: 콤보 태그 회수 및 수명 주기 정석 마감 마킹
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
        
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 3초 내에 마우스를 다시 클릭하지 않으면 스택을 폭파 소각할 타임아웃 타이머를 예약 구동합니다.
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
        
        // 💡 [연타 감시망 타설]: 플레이어가 다음 입력을 누르는 순간을 실시간 포착하여 스스로를 재귀 격발시킵니다.
        UAbilityTask_WaitInputPress* InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, true);
        if (InputTask)
        {
            // 다음 연타 클릭 감지 시, 수명선이 유지된 상태로 ActivateAbility의 상단 전방 배관으로 시퀀스를 회전시킵니다.
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
    // 마우스 재연타 포착 즉시, 수명 주기가 유지된 상태에서 루프 연타 연산을 다시 가동합니다.
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

    // 💡 타임아웃으로 콤보가 중도 분쇄 파쇄되었다면 비로소 여기서 어빌리티 독점권을 해제 반환합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UFT_AladdinUltimateAbility::ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex)
{
    if (!OwnerActor || !SourceASC || !OwnerActor->GetWorld()) return;

    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    ForwardVector.Z = 0.f; // 고저차 왜곡 차단
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
            if (!TargetActor) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
            if (TargetASC)
            {
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                float FinalDamage = BaseDamageValue;
                
                // 자비의 신기루 증강 카드 보유 상태에서 막타(3타) 적중 시 대미지 2배 증폭 주입
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
    // 기절 등 하드 CC기에 노출되어 캐스팅 취소 시 타이머 및 유령 스택 완벽 소각 수거
    if (GetWorld() && ComboTimeoutTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
        ComboTimeoutTimerHandle.Invalidate();
    }

    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (SourceASC)
    {
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
    }
    CurrentWishCount = 0;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}