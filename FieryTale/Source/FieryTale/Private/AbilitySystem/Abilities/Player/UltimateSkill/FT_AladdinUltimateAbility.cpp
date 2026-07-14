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
    // 본체 개체별 독립적인 상태 추적 및 타이머 제어를 위해 인스턴싱 정책을 확정합니다.
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
    // 이미 지니 콤보가 가동 중인 연타 윈도우 상태라면 쿨다운 및 코스트 조건을 우회하여 즉시 진입을 승인합니다.
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

    // 연속 입력 타이머 초기화: 연타 입력 시 기존의 3초 제한 유효 시간선을 청정하게 갱신합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
    }

    // 콤보 활성화 상태 확인 (최초 진입과 연속 입력 분기점을 판별합니다)
    bool bIsFirstWish = !SourceASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);

    if (bIsFirstWish)
    {
        // 최종 버그 완치선 - 부모 가드 및 커밋 격리 타설
        // 재귀 루프 중 중복 격발을 차단하기 위해 Super 호출과 CommitAbility 파이프라인을 오직 1타 최초 진입 시점에만 단 한 번 작동하도록 가두어 락인합니다.
        // 이로써 2타, 3타 타격 대미지 격발 시 게이지가 유령 리필되는 누수 버그가 100% 영구 박멸됩니다.
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
        // 3초 골든타임 유효 시간 내에 연타 성공 시 스택 카운트를 전진시킵니다.
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

    // 전방 지니 강타 박스 트레이스 영역 연산을 실행합니다.
    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    if (CurrentWishCount >= 3)
    {
        // 대망의 3타 최종 마감: 콤보 태그 회수 및 수명 주기 정석 마감 마킹을 수행합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
        
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 다음 연타 스택 유효 제한 시간(3.0초)을 재예약합니다.
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
        
        // 순정 GAS 입력 대기 태스크를 가동하여 플레이어의 추가 키 입력을 대기 선로에 올립니다.
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
    // 입력 감지 시 액티베이션 선로로 재귀 진입시켜 연속 타격을 전개합니다.
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
    ForwardVector.Z = 0.f; // 고저차 지형에 의한 전방 궤적 왜곡을 수평 보정합니다.
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
            //  [대량 학살 시 성능 병목 파쇄 - 마스터 계산서 1장 선제 발행]
            // 루프 바깥에서 공용 스펙 장부를 단 1회만 힙 할당하여 고속 정산 파이프라인을 구축합니다.
            FGameplayEffectSpecHandle MasterSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
            if (MasterSpecHandle.IsValid() && MasterSpecHandle.Data.IsValid())
            {
                // 가해자(Source/Instigator) 정보를 마스터 컨텍스트에 박제
                MasterSpecHandle.Data->GetContext().AddInstigator(OwnerActor, OwnerActor);
                MasterSpecHandle.Data->GetContext().AddSourceObject(OwnerActor);

                for (const FOverlapResult& Result : OverlapResults)
                {
                    AActor* TargetActor = Result.GetActor();
                    if (!IsValid(TargetActor)) continue;

                    UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();
                    if (TargetASC)
                    {
                        // AOS 피아식별 안전망: 아군 사선 관통 및 오사 차단
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                        if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                        // 타깃 사망 시 과격 시체 구타 방지
                        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

                        // 증강 카드 및 3타 여부에 따른 대미지 정산
                        float FinalDamage = BaseDamageValue;
                        if (SmashIndex == 3 && SourceASC->HasMatchingGameplayTag(FTTags::FTAugments::Applied_Aladdin_MercyMirage))
                        {
                            FinalDamage *= 2.0f;
                        }

                        //  [초고속 장부 돌려막기 적용]: 복사 생성자를 기반으로 가벼운 스택 사본을 찍어내어 타격 주입
                        FGameplayEffectSpec LocalSpec(*MasterSpecHandle.Data.Get());
                        FGameplayEffectContextHandle LocalContext = LocalSpec.GetContext().Duplicate();

                        // 피해 가중치(FinalDamage)를 동적으로 사본 장부에 덧씌웁니다.
                        LocalSpec.SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);

                        // 타깃 적중 히트 결과 기록
                        FHitResult IndividualHit;
                        IndividualHit.HitObjectHandle = TargetActor;
                        IndividualHit.Location = TargetActor->GetActorLocation();
                        IndividualHit.ImpactPoint = TargetActor->GetActorLocation();
                        
                        LocalContext.AddHitResult(IndividualHit, true);
                        LocalSpec.SetContext(LocalContext);

                        //  공격자의 시스템으로 타깃의 ASC 장부에 직통 대미지 집행!
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
        // 자가 수급 전선 초기화 마감: 어빌리티 최종 마감 시점에 글로벌 궁극기 분류 태그를 청정하게 동시 해제 수거합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTAbilities::UltimateSkill);
    }
    CurrentWishCount = 0;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}