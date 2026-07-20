// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AladdinUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h" 
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h" 
#include "GameplayTags/FTTags.h"

UFT_AladdinUltimateAbility::UFT_AladdinUltimateAbility()
    : AttackRange(1500.0f)
    , AttackWidth(300.0f)
    , BaseDamageValue(50.0f)
    , CurrentWishCount(0)
{
    // 개체별 상태 추적을 위해 인스턴싱 정책을 적용합니다.
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
    // 콤보가 활성화된 상태라면 조건 검증 없이 통과시킵니다.
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

    // 콤보 타이머를 초기화합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
    }

    // 첫 시전인지 확인합니다.
    bool bIsFirstWish = !SourceASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);

    if (bIsFirstWish)
    {
        // 첫 시전 시에만 부모 클래스의 ActivateAbility를 호출하고 활성화 조건을 검증합니다.
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
        // 콤보 카운트를 증가시킵니다.
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

    UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
    if (LoadedMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("AladdinUltMontage"), LoadedMontage, 1.0f);
        if (MontageTask)
        {
            MontageTask->OnCompleted.AddDynamic(this, &UFT_AladdinUltimateAbility::OnMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_AladdinUltimateAbility::OnMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_AladdinUltimateAbility::OnMontageFinished);
            MontageTask->ReadyForActivation();
        }
    }

    // 강타 판정을 실행합니다.
    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    if (CurrentWishCount >= 3)
    {
        // 최대 콤보 도달 시 어빌리티 상태만 초기화하고, 어빌리티 종료는 몽타주가 끝날 때 수행합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
    }
    else
    {
        // 콤보 제한 시간을 설정합니다.
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

        // 키 입력 대기 태스크를 시작합니다.
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

void UFT_AladdinUltimateAbility::OnMontageFinished()
{
    // 애니메이션이 끝나면 어빌리티를 종료합니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AladdinUltimateAbility::OnComboInputPressed(float TimeWaited)
{
    // 입력이 감지되면 어빌리티를 다시 활성화합니다.
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
    if (!IsValid(OwnerActor) || !SourceASC || !OwnerActor->GetWorld())
    {
        return;
    }

    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    ForwardVector.Z = 0.f; // Z축 변위를 0으로 고정하여 수평 방향을 유지합니다.
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
    
    if (OwnerActor->HasAuthority())
    {
        bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
            OverlapResults, BoxCenter, BoxOrientation, ECollisionChannel::ECC_Pawn, ScanBox, QueryParams
        );

        if (bHasOverlap && DamageGameplayEffectClass)
        {
            // 루프 외부에서 이펙트 인스턴스를 한 번만 생성하여 최적화합니다.
            FGameplayEffectSpecHandle MasterSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
            if (MasterSpecHandle.IsValid() && MasterSpecHandle.Data.IsValid())
            {
                // 시전자 정보를 컨텍스트에 추가합니다.
                MasterSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                MasterSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);

                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* TargetActor = Result.GetActor();
                    if (!IsValid(TargetActor))
                    {
                        continue;
                    }

                    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
                    if (TargetASC && SourceASC)
                    {
                        // 아군 타격을 방지합니다.
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
                        {
                            continue;
                        }
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
                        {
                            continue;
                        }

                        // 대상이 이미 사망했는지 확인합니다.
                        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
                        {
                            continue;
                        }

                        // 증강 및 콤보에 따른 대미지 보정 연산입니다.
                        float FinalDamage = BaseDamageValue;
                        if (SmashIndex == 3 && SourceASC->HasMatchingGameplayTag(FTTags::FTAugments::Applied_Aladdin_MercyMirage))
                        {
                            FinalDamage *= 2.0f;
                        }

                        // 생성된 이펙트 인스턴스를 재사용하여 대미지를 적용합니다.
                        FGameplayEffectSpec LocalSpec(*MasterSpecHandle.Data.Get());
                        FGameplayEffectContextHandle LocalContext = LocalSpec.GetContext().Duplicate();

                        // 보정된 대미지를 적용합니다.
                        LocalSpec.SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);

                        // 타깃 적중 히트 결과 기록
                        FHitResult IndividualHit;
                        IndividualHit.HitObjectHandle = TargetActor;
                        IndividualHit.Location = TargetActor->GetActorLocation();
                        IndividualHit.ImpactPoint = TargetActor->GetActorLocation();
                        
                        LocalContext.AddHitResult(IndividualHit, true);
                        LocalSpec.SetContext(LocalContext);

                        // 대미지를 적용합니다.
                        SourceASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
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
        // 어빌리티 종료 시 궁극기 태그를 제거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }
    CurrentWishCount = 0;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}