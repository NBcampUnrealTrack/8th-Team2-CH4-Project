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
    : AttackRange(1500.0f)     // 기획 데이터 락인: 지니 환영이 전방으로 전진하며 타격할 총 사거리 15미터 명세
    , AttackWidth(300.0f)      // 기획 데이터 락인: 직선 강타 박스 판정의 좌우 반폭 수치 (총 너비 6미터 광역 판정)
    , BaseDamageValue(50.0f)   // 기획 데이터 락인: 강타 1회 적중 시 적용할 기본 기저 피해량 수치 명세
    , CurrentWishCount(0)      // 현재 콤보 단계를 추적할 내부 트래커 변수 초기화
{
    // 인스턴싱 정책: 영웅 캐릭터마다 고유의 콤보 카운트 및 3초 제한 시간 타이머를 격리 제어하기 위해 인스턴스화합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 클라이언트 뷰에서 지니 강타 이펙트와 동선이 켜지도록 선입력 예측을 가동합니다.
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

    // 기존 타이머 클리어: 1타 혹은 2타 격발 후 3초가 지나기 전에 다음 Q 입력을 성공했다면 기존 수거 타이머를 취소합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
    }

    // 콤보 윈도우 판정: 시전자 몸에 AladdinComboActive 버프 태그가 없다면 최초 시전인 1타 상태로 판정합니다.
    bool bIsFirstWish = !SourceASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);

    if (bIsFirstWish)
    {
        // 콤보 혁신 배관: 대망의 1타 시점에만 궁극기 마스터 자원 및 게이지 비용 소모를 공식 커밋합니다.
        if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
        {
            // 게이지가 부족하면 즉시 반환 종료합니다.
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
        CurrentWishCount = 1;
    }
    else
    {
        // 3초 유효 시간 내에 재진입했다면 게이지를 추가 소모하지 않고 콤보 스택 스케줄만 전진시킵니다.
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

    // 지니 환영 강타 실행: 계산식과 실시간 오버랩 판정 구역으로 캐릭터 포인터와 콤보 단계를 넘깁니다.
    ExecuteGenieSmash(OwnerActor, SourceASC, CurrentWishCount);

    // 콤보 윈도우 유효 시간 및 정리 파이프라인 제어 구역
    if (CurrentWishCount >= 3)
    {
        // 막타 완료 분기: 3번째 소원까지 모두 완수했다면 시전자 몸에서 콤보 대기 태그를 회수하고 스택을 원상태로 청소합니다.
        SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        CurrentWishCount = 0;
        
        // 중복 호출 릭이 없도록 명확히 분기문을 갈라 단독 정상 종료 관문을 가동합니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 연사 대기 분기: 1타 또는 2타 타격 직후에는 시전자 몸에 다음 콤보 가능 표식 태그를 루즈하게 부착합니다.
        SourceASC->AddLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        
        // 버그 방어선 완착: 3초 내에 다음 Q 입력을 하지 않아 콤보가 끊기면 자동으로 좀비 스택과 태그를 수거하는 안전 벨브 타이머를 가동합니다.
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
        
        // 중요: 능력을 즉시 반환 소멸시켜 주어야 키 입력 컴포넌트가 다음 Q 연타 신호를 차단하지 않고 재진입 통로를 열어줍니다.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UFT_AladdinUltimateAbility::ResetComboState()
{
    // 안전 수거 함수: 3초 입력 타임아웃 도달 시 백그라운드에서 유령 잔재 태그와 스택 카운트를 흔적도 없이 소멸시킵니다.
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
    
    // 박스 판정의 수평 중심점 연산
    FVector BoxCenter = StartLocation + (ForwardVector * (AttackRange * 0.5f));
    
    // 고저차 보정 완착: AOS 한타 교전 도중 계단이나 미세 언덕 지형에서 궁극기 판정이 통째로 증발하는 물리 릭을 막기 위해 Z축 반폭 범위를 250으로 넉넉히 확장합니다.
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

    // 가시성 및 캐릭터 충돌 폰 채널을 기반으로 전방 광역 박스 오버랩 스캔을 격발합니다.
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
                // 피아식별 필터링: 블루 팀 대 레드 팀 진영 태그를 대조하여 아군 오사 피해 주입을 완벽 차단합니다.
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                float FinalDamage = BaseDamageValue;
                
                // 증강 카드 링크 연동: 대망의 3타 막타 격발 시점에 알라딘 전용 자비의 신기루 증강 카드를 선택하여 보유 중이라면 대미지 폭발 가산 2배율을 주입합니다.
                if (SmashIndex == 3 && SourceASC->HasMatchingGameplayTag(FTTags::FTAugments::Applied_Aladdin_MercyMirage))
                {
                    FinalDamage *= 2.0f;
                }

                // 에셋 바인딩 가이드
                //
                // 에디터 세팅: 에디터 패널의 DamageGameplayEffectClass 슬롯에 피해 연산용 마스터 이펙트 에셋을 지정해야 합니다.
                // 런타임 흐름: 최종 연산된 기획 피해량 수치(FinalDamage)를 FTCombat_Damage 우체통 주소지에 담아
                //              마스터 속성 집합의 Damage 속성으로 발송합니다. 최종 처리는 속성 집합에서 안전 전담합니다.
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
    // 인터럽트 방어선 완료: 3단 콤보 시전 도중에 적의 하드 CC기를 맞아 비정상 취소 하차당하면 가동 중이던 3초 타이머를 끄고 콤보 태그와 스택 카운트를 그 즉시 전량 청소 수거하여 불사조 버그를 차단합니다.
    if (bWasCancelled)
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(ComboTimeoutTimerHandle);
        }
        
        UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
        if (SourceASC)
        {
            SourceASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::AladdinComboActive);
        }
        CurrentWishCount = 0;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}