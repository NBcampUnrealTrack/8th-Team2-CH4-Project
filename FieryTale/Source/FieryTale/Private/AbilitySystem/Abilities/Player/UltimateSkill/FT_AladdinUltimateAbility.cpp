// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AladdinUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"

UFT_AladdinUltimateAbility::UFT_AladdinUltimateAbility()
{
    // FieryTale 공통 스킬 규격 세팅
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);
}

void UFT_AladdinUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActor* OwnerActor = ActorInfo->AvatarActor.Get();
    UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
    
    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 첫 번째 시전 시에만 부모의 코스트(궁극기 게이지 소모 등)를 처리하여 자원을 리셋합니다
    if (CurrentWishCount == 0)
    {
        CommitAbilityCost(Handle, ActorInfo, ActivationInfo);
        Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    }

    // 소원 스택 카운트 증가 (1 -> 2 -> 3)
    CurrentWishCount++;

    if (bDrawDebugs)
    {
        // Kismet 형식을 배제하고 순정 FString 연산으로만 안전하게 로깅 배관을 뚫어줍니다
        FString DebugMsg = FString::Printf(TEXT("Aladdin Ultimate: 지니 %d번째 소원을 빌게"), CurrentWishCount);
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
        }
        
        UE_LOG(LogTemp, Log, TEXT("%s"), *DebugMsg);
    }

    // --- 🧞 지니 환영 강타 실행 ---
    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    // 3번째 소원까지 전부 소모했으면 시퀀스를 안전하게 완전 종료하고 스택을 리셋합니다
    if (CurrentWishCount >= MaxWishes)
    {
        CurrentWishCount = 0; // 다음 턴을 위해 리셋
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 1, 2번째 소원일 때는 EndAbility를 부르지 않고 내부 활성화 상태를 유지합니다
        // 플레이어가 제한 시간 내에 궁극기 키를 다시 누르면 ActivateAbility가 재진입합니다
    }
}

void UFT_AladdinUltimateAbility::ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex)
{
    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    
    // 전방 15m 직선 박스 영역의 중심점 계산
    FVector BoxCenter = StartLocation + (ForwardVector * (AttackRange * 0.5f));
    
    // 박스의 절반 크기 (두께 100, 좌우 폭, 전방 길이 절반)
    FVector BoxHalfExtent = FVector(AttackRange * 0.5f, AttackWidth, 100.f);
    FQuat BoxOrientation = OwnerActor->GetActorQuat();

    TArray<FOverlapResult> OverlapResults;
    FCollisionShape ScanBox = FCollisionShape::MakeBox(BoxHalfExtent);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    // 디버그 드로잉 (느린 범위 공격 박스 시각화)
    if (bDrawDebugs && OwnerActor->GetWorld())
    {
        DrawDebugBox(OwnerActor->GetWorld(), BoxCenter, BoxHalfExtent, BoxOrientation, FColor::Blue, false, 1.f, 0, 3.f);
    }

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
                // [이중 방어선 1단계] 물리 스캔 단에서 아군 오사 차단 (디버프 릭 원천 방쇄)
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // --- [기획 변경 대응 확장 구역] 증강(Augment) 분기 처리 거점 ---
                float FinalDamage = BaseDamageValue; // 기본 50
                
                // [알라딘 전용 증강 예시]: 3번째 소원은 지니가 자비 없이 2배로 내리침
                if (SmashIndex == 3 && SourceASC->HasMatchingGameplayTag(FTTags::FTAugments::Applied_Aladdin_MercyMirage))
                {
                    FinalDamage *= 2.0f;
                }

                // --- 이펙트 발행 및 고정 대미지 버퍼 적재 ---
                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        // 계산기 전용 공용 대미지 태그에 최종 대미지 주입
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, FinalDamage);
                        
                        // 타깃에 무결하게 직통 적용
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }
            }
        }
    }
}