// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    // 인스턴싱 정책: 영웅 캐릭터마다 고유의 궁극기 스펙과 범위를 격리 관리하기 위해 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 키 입력 즉시 로컬 뷰에서 늑대의 하울링 시각 효과가 켜지도록 클라이언트 선입력 예측 시스템을 구동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 에셋 태그 주입: 궁극기 범주 분류를 위해 전용 에셋 태그를 식별자로 완착합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill);
    SetAssetTags(AssetTags);

    // 기획 사양 기본값 안전 세팅 구역
    HuntRadius = 800.0f;       // 부채꼴 포효가 뻗어나갈 최대 사거리 8미터 명세
    ConeAngle = 90.0f;         // 전방 조준선 기준 좌우 각각 45도씩 펼쳐지는 총 90도 부채꼴 각도 명세
    BaseDamageValue = 150.0f;  // 공포의 포효에 적중당한 대상에게 가동할 기저 피해량 수치 명세
}

void UFT_WolfRoarAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // GAS 마스터 규격: 마스터 베이스 클래스의 게이지 비용 소모 파이프라인(ApplyCost)을 가동하여 게이지 소모 및 검증을 최상단 관문에서 처리합니다.
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

    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("WolfRoar Activated: 할머니를 삼켰던 이빨로 적들을 물어뜯어라."));
    }

    // --- 전방 부채꼴 볼륨 확정 타격 물리 스캔 구역 ---
    FVector StartLocation = OwnerActor->GetActorLocation();
    FVector ForwardVector = OwnerActor->GetActorForwardVector();
    ForwardVector.Z = 0.f; // 3차원 고저차 경사로로 인한 전방 조준선 평면 왜곡을 원천 선제 차단합니다.
    ForwardVector.Normalize();

    TArray<FOverlapResult> OverlapResults;
    // 1차 필터링: 부채꼴 리치 반경에 맞추어 8미터 원형 구체 형태의 1차 충돌 풀을 확보합니다.
    FCollisionShape ScanSphere = FCollisionShape::MakeSphere(HuntRadius);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        StartLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_Pawn, 
        ScanSphere,
        QueryParams
    );

    if (bHasOverlap)
    {
        // 삼각함수 물리 연산: 기획 지정 부채꼴 각도의 절반값을 코사인 라디안 값으로 변환하여 내적 유효 임계 임계치 기준선을 수립합니다.
        float DotThreshold = FMath::Cos(FMath::DegreesToRadians(ConeAngle * 0.5f));

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* TargetActor = Result.GetActor();
            if (!TargetActor) continue;

            // 판정 버그 방어선 완착: 높낮이가 다른 2층 지형이나 공중 점프 상태의 적도 수평 평면상에서 공평하게 부채꼴 판정을 받도록 Z축 편차를 강제 소멸시킵니다.
            FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation);
            TargetDirection.Z = 0.f; 
            TargetDirection.Normalize();

            // 시전자 정면 시선 벡터와 적 캐릭터 위치 방향 벡터 간의 내적 연산을 단행합니다.
            float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

            // 부채꼴 범위 유효성 검증: 변환 임계치보다 내적 수치가 낮다면 시야각 90도 외곽에 존재하는 적이므로 스킵 처리합니다.
            if (CurrentDot < DotThreshold) continue;

            UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();

            if (TargetASC)
            {
                // 이중 방어선: 물리 스캔 풀에 포착되었더라도 아군 진영 태그를 대조하여 한타 중 아군 오사 피해 및 속박 디버프 릭을 원천 방쇄합니다.
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                // 에셋 바인딩 가이드: 대미지 이펙트 에셋
                // 에디터 내 DamageGameplayEffectClass 슬롯에 포효 피해용 이펙트를 장착하십시오.
                // 런타임 흐름: BaseDamageValue 150 수치를 피해 연산 우체통에 실어 마스터 속성 집합으로 배달합니다.
                if (DamageGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                        TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                    }
                }

                // 에셋 바인딩 가이드: 군중 제어 속박 이펙트 에셋
                // 에디터 내 RootGameplayEffectClass 슬롯에 지속시간 2초 및 이동 불가 상태 이상 태그가 포함된 이펙트를 장착하십시오.
                // 런타임 흐름: 조건이 일치하는 부채꼴 내 모든 적들의 발을 2초간 강제로 묶어 무력화합니다.
                if (RootGameplayEffectClass)
                {
                    FGameplayEffectSpecHandle RootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                    if (RootSpecHandle.IsValid())
                    {
                        TargetASC->ApplyGameplayEffectSpecToSelf(*RootSpecHandle.Data.Get());
                    }
                }
            }
        }
    }

    // 포효 격발 및 디버프 주입 시퀀스가 완료되면 수명 주기를 깔끔하게 종료합니다.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}