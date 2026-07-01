// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_AliceUltimateAbility.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UFT_AliceUltimateAbility::UFT_AliceUltimateAbility()
{
    // 인스턴싱 정책: 캐릭터마다 궁극기 타이머 핸들과 활성화 상태를 독립 제어하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬에서 시간 정지 영역 이펙트가 표현되도록 클라이언트 선입력 예측 시스템을 기동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    // 시전자 락: 앨리스가 궁극기를 펼치고 주문을 외우는 1초 동안은 본인도 채널링 상태임을 태그로 명시합니다.
    // 런타임 흐름 요약: 이 태그가 켜진 동안 캐릭터 이동 컴포넌트나 다른 스킬 허브가 앨리스의 독단 조작을 임시 차단합니다.
    ActivationOwnedTags.AddTag(FTTags::FTCombat::Skill_Channelling);
}

void UFT_AliceUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 무한 시전 핵 차단: 자원 상태를 검증하고 궁극기 게이지 비용을 서버와 클라이언트에서 공식 소모합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("\"아직 6시야! 무대는 끝나지 않아!\""));
    }

    // --- 🕒 1단계: 자신 중심 광역 시간 정지 원형 스캔 구역 🕒 ---
    FVector CenterLocation = OwnerActor->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    // 기획 데이터 락인: 에디터 변수 TimeStopRadius 수치를 기반으로 완전 구체 형태의 충돌 셰이프를 조립합니다.
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(TimeStopRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

#if !UE_BUILD_SHIPPING
    if (bDrawDebugs)
    {
        DrawDebugSphere(OwnerActor->GetWorld(), CenterLocation, TimeStopRadius, 32, FColor::Purple, false, 2.f, 0, 2.f);
    }
#endif

    // 가시성 및 폰 채널을 기반으로 자신 주변의 모든 캐릭터를 실시간 캡처합니다.
    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        CenterLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_Pawn,
        ScanSphere,
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
                // 이중 방어선: 아군 진영 태그를 실시간 대조하여 다과회 영역 내의 아군 오사 기절 피해를 완벽 차단합니다.
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // 에셋 바인딩 가이드
                // 
                // 에디터 세팅: 에디터 내 TimeStopDebuffEffectClass 슬롯에 기절 상태 태그가 포함된 디버프 이펙트 에셋을 장착해야 합니다.
                // 런타임 흐름: 포착된 모든 적들의 몸에 2초간 지속되는 시간 정지 상태 기절 이펙트를 다이렉트로 주입합니다.
                if (TimeStopDebuffEffectClass)
                {
                    FGameplayEffectSpecHandle DebuffSpecHandle = MakeOutgoingGameplayEffectSpec(TimeStopDebuffEffectClass, GetAbilityLevel());
                    if (DebuffSpecHandle.IsValid())
                    {
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DebuffSpecHandle.Data.Get());
                    }
                }
            }
        }
    }

    // --- 2단계: 안전한 오브젝트 포인터 콜백 타이머 구동 구역 ---
    // 물리 크래시 방어 완착: 런타임에 소멸 위험이 매우 높은 원시 포인터 복사 방식의 람다 캡처 배관을 전면 철거했습니다.
    // 인스턴싱 빌드업된 클래스 고유의 멤버 변수와 UFUNCTION 명시적 콜백 주소를 결합하여 메모리 안정성을 영구 보장합니다.
    if (GetWorld())
    {
        // [보정 완료] 존재하지 않는 로컬 변수 'World'를 도려내고, 순정 엔진 API인 GetWorld() 배관으로 정확히 치환합니다.
        // 1초 뒤에 앨리스가 적들보다 먼저 행동 불가를 풀고 자유를 얻도록 타이머를 구동합니다.
        GetWorld()->GetTimerManager().SetTimer(
            AliceReleaseTimerHandle,
            this,
            &UFT_AliceUltimateAbility::ReleaseAlice,
            1.0f,
            false
        );
    }
}

void UFT_AliceUltimateAbility::ReleaseAlice()
{
    // 장전 해제 파이프라인: 앨리스가 1초 뒤 먼저 자유의 몸이 되는 순간 가동됩니다.
    // GAS 프레임워크가 인스턴스 빌드업 시 내부에 실시간 관리해 주는 안전 수명선 구조체 변수를 정석 활용합니다.
    if (!CurrentActorInfo) return;

    if (bDrawDebugs && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("앨리스 행동 가능! 프리딜 타임 시작!"));
    }
    
    // 이 능력을 정상 종료하면 생성자 구역의 ActivationOwnedTags에 엮여있던 Skill_Channelling 태그가 안전하게 자동 소멸합니다.
    // 앨리스는 먼저 풀려나 프리딜을 시작하고, 피격된 적들은 주입받은 디버프 지속시간에 의해 아직 1초 더 기절 상태로 굳어있게 됩니다.
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceUltimateAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 인터럽트 예외 방어: 앨리스가 1초 타이머가 끝나기 전에 적의 다른 하드 CC를 맞아 비정상 취소되더라도 가동 중이던 타이머 잔재 핸들을 완벽하게 청소 수거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AliceReleaseTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}